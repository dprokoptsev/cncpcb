#include "errors.h"
#include "utility.h"
#include "geom.h"
#include "cnc.h"
#include "keyboard.h"
#include "gcode.h"
#include "settings.h"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <utility>
#include <algorithm>

#include <unistd.h>


void cnc_machine::rebind(std::iostream& s)
{
    s_ = &s;

    wco_ = vector();
    position_ = point();
    feed_rate_ = spindle_speed_ = 0;
    spindle_on_ = false;
    touches_ground_ = false;
    idle_ = true;

    reset();
    get_status();
    
    if (alarm_) {
        std::cerr << "\rHoming required" << std::endl;
    } else {
        talk("G21");
        talk("G90");
    }
}
    
void cnc_machine::reset()
{
    s_->clear();
    *s_ << "\x18" << std::flush;
    std::string line;
    while (std::getline(*s_, line) && !starts_with(line, "Grbl ")) {
        if (settings::g_params.dump_wire)
            std::cerr << "\r\033[32m... recv: " << line << "\033[0m" << std::endl;
    }
    if (interactive::interrupted())
        throw std::runtime_error("user break");
    if (!*s_)
        throw grbl_error::protocol_violation();
    
    spindle_speed_ = 0;
    spindle_on_ = false;
    get_status();
}

void cnc_machine::get_status()
{
    touches_ground_ = false;
    idle_ = false;
    alarm_ = false;
    
    std::string status;
    while (status.empty()) {
        auto st = talk("?");
        auto i = std::find_if(
            st.begin(), st.end(),
            [](const std::string& s) { return !s.empty() && s[0] == '<'; }
        );
        if (i != st.end())
            status = i->substr(1);
    }

    point wpos;
    point mpos;
    for (const std::string& field: split<std::string>(status, "|")) {
        if (field.substr(0, 4) == "Idle") {
            idle_ = true;
        } else if (field == "Alarm") {
            alarm_ = true;
        } else if (field.substr(0, 5) == "WPos:") {
            wpos = point(field.substr(5));
        } else if (field.substr(0, 5) == "MPos:") {
            mpos = point(field.substr(5));
        } else if (field.substr(0, 4) == "WCO:") {
            wco_ = vector(field.substr(4));
        } else if (field.substr(0, 3) == "FS:") {
            auto fs = split<double>(field.substr(3), ",");
            spindle_on_ = (fs.size() >= 2 && fs[1] >= 1);
        } else if (field.substr(0, 3) == "Pn:") {
            std::string s = field.substr(3);
            touches_ground_ = (s.find('P') != std::string::npos);
        }
    }
    
    if (wpos.defined()) {
        position_ = wpos;
    } else if (mpos.defined()) {
        position_ = mpos - wco_;
    } else {
        throw grbl_error::protocol_violation();
    }
}

point cnc_machine::position()
{
    if (!position_.defined())
        get_status();
    return position_;
}

point cnc_machine::absolute_position()
{
    return position() + wco();
}

bool cnc_machine::touches_ground()
{
    get_status();
    return touches_ground_;
}

void cnc_machine::wait()
{
    bool dump = settings::g_params.dump_wire;
    settings::g_params.dump_wire = false;
    for (;;) {
        get_status();
        if (idle_)
            break;
        usleep(50000);
    }
    settings::g_params.dump_wire = dump;
}

std::string cnc_machine::read_hash(const std::string& name, size_t index)
{
    wait();
    for (const std::string& line: talk("$#")) {
        auto fields = split<std::string>(line, ":");
        if (fields.size() > index && fields[0] == name)
            return fields[index];
    }
    throw grbl_error::protocol_violation();
}

vector cnc_machine::wco()
{
    if (!wco_.defined())
        wco_ = vector(read_hash("[G54", 1));
    return wco_;
}

void cnc_machine::redefine_position(point newpos)
{
    talk("G10 P0 L20 " + newpos.grbl());
    wco_ = vector();
    position_ = newpos;
}

void cnc_machine::move(point p, cnc_machine::move_mode m)
{
    if (m == move_mode::safe && p.z < MIN_SAFE_HEIGHT) {
        move_z(MIN_SAFE_HEIGHT);
        p.z = MIN_SAFE_HEIGHT;
    }

    talk("G0 " + p.grbl());
    position_ = p;    
}

void cnc_machine::move_xy(point p, cnc_machine::move_mode m)
{
    move({ p.x, p.y, position().z }, m);
}

void cnc_machine::move_z(double z, cnc_machine::move_mode m)
{
    if (m == move_mode::safe)
        z = std::max(z, 0.1);
    
    talk("G0 Z" + std::to_string(z));
    position_.z = z;
}

void cnc_machine::feed(point p)
{
    talk("G1 " + p.grbl());
    position_ = p;
}   

void cnc_machine::feed_z(double z)
{
    talk("G1 Z" + std::to_string(z));
    position_.z = z;
}

double cnc_machine::probe()
{
    position_ = point();
    double prev_feed_rate = feed_rate();
    talk("G38.2 F15 Z" + lexical_cast<std::string>(-max_travel().z - wco().z + 1));
    wait();
    double ret = (point(read_hash("[PRB", 1)) - wco()).z;
    set_feed_rate(prev_feed_rate);
    return ret;
}

class cnc_machine::tmpwcs {
public:
    tmpwcs(cnc_machine& cnc, int wcs):
        cnc_(&cnc), wcs_(cnc.wcs_)
    { cnc_->select_wcs(wcs); }
    
    ~tmpwcs() { cnc_->select_wcs(wcs_); }
    
    tmpwcs(const tmpwcs&) = delete;
    tmpwcs& operator = (const tmpwcs&) = delete;
    
private:
    cnc_machine* cnc_;
    int wcs_;
};

void cnc_machine::home(vector axis)
{
    std::string axis_name;
    if ((axis - vector::axis::x()).length() < 1e-5)
        axis_name = "X";
    else if ((axis - vector::axis::y()).length() < 1e-5)
        axis_name = "Y";
    else if ((axis - vector::axis::z()).length() < 1e-5)
        axis_name = "Z";
    else
        throw std::runtime_error("unknown axis: " + lexical_cast<std::string>(axis));
    
    talk("$H" + axis_name);
}

void cnc_machine::home()
{
    talk("$X");
    
    home(vector::axis::z());
    move_z(-0.1 - wco().z, move_mode::unsafe);
    wait();
    home(vector::axis::x());
    home(vector::axis::y());
    
    talk("G21");
    talk("G90");
}

void cnc_machine::select_wcs(int wcs)
{
    if (wcs < 0 || wcs > 5)
        throw std::runtime_error("WCS must be in range 0..5");
    talk("G" + std::to_string(54 + wcs));
    
    wcs_ = wcs;
    wco_ = vector();
    position_ = point();
}

void cnc_machine::set_spindle_speed(double speed)
{
    talk("S" + std::to_string(speed));
    spindle_speed_ = speed;
}

bool cnc_machine::is_spindle_on()
{
    get_status();
    return spindle_on_;
}

void cnc_machine::set_spindle_on()
{
    if (!spindle_speed_)
        set_spindle_speed(100);
    talk("M3");
    spindle_on_ = true;
}

void cnc_machine::set_spindle_off()
{
    talk("M5");
    spindle_on_ = false;
}

void cnc_machine::set_feed_rate(double feed_rate)
{
    feed_rate_ = feed_rate;
    talk("F" + std::to_string(feed_rate));
}

void cnc_machine::dwell(double seconds) { talk("G4 P" + std::to_string(seconds)); }

void cnc_machine::send_gcmd(const gcmd& cmd)
{
    talk(lexical_cast<std::string>(cmd));
    position_ = point();
}

std::vector<std::string> cnc_machine::talk(const std::string& cmd)
{
    if (settings::g_params.dump_wire)
        std::cerr << "\r\033[33m... send: " << cmd << "\033[0m" << std::endl;

    *s_ << cmd << std::endl;

    std::vector<std::string> resp;
    std::string line;
    while (std::getline(*s_, line)) {
        if (interactive::interrupted())
            throw std::runtime_error("user break");
        
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.pop_back();

        if (settings::g_params.dump_wire)
            std::cerr << "\r\033[32m... recv: '" << line << "'\033[0m" << std::endl;

        if (line == "ok") {
            return resp;
        } else if (starts_with(line, "error:")) {
            throw grbl_error(lexical_cast<int>(line.substr(6)));
        } else if (starts_with(line, "ALARM:")) {
            usleep(500000);
            throw grbl_alarm(lexical_cast<int>(line.substr(6)));
        } else if (line.size() >= 2 && line[0] == '<' && line[line.size() - 1] == '>') {
            resp.push_back(line.substr(0, line.size() - 1));
        } else if (line.size() >= 2 && line[0] == '[' && line[line.size() - 1] == ']') {
            resp.push_back(line.substr(0, line.size() - 1));
        } else if (starts_with(line, "Grbl ")) {
            // CNC was reset by front-panel killswitch.
            reset(); // Clear what we may have sent afterwards
            throw grbl_reset();
        } else if (starts_with(line, "$") && line.find('=') != std::string::npos) {
            resp.push_back(line);
        } else {
            continue;
        }
    }

    if (interactive::interrupted())
        throw std::runtime_error("user break");

    throw grbl_error::protocol_violation();
}

double cnc_machine::setting(int index)
{
    if (settings_.empty()) {
        wait();
        for (std::string s: talk("$$")) {
            size_t eq;
            if (s.empty() || s[0] != '$' || (eq = s.find('=')) == std::string::npos)
                continue;
            settings_[lexical_cast<int>(s.substr(1, eq-1))] = lexical_cast<double>(s.substr(eq+1));
        }
    }
    
    auto i = settings_.find(index);
    return (i != settings_.end()) ? i->second : 0;
}

vector cnc_machine::vector_setting(int index)
{
    return {
            setting(index),
            setting(index + 1),
            setting(index + 2)
    };
}

vector cnc_machine::mask_setting(int index)
{
    int mask = (int) setting(index);
    return {
            (mask & 1) ? -1.0 : 1.0,
            (mask & 2) ? -1.0 : 1.0,
            (mask & 4) ? -1.0 : 1.0
    };
}
