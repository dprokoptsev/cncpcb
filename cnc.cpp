#include "errors.h"
#include "utility.h"
#include "geom.h"
#include "cnc.h"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <utility>


extern bool g_dump_wire;

cnc_machine::cnc_machine(std::iostream& s): s_(&s)
{
    for (;;) {
        std::string line;
        if (!std::getline(*s_, line))
            throw grbl_error::protocol_violation();
        if (line.substr(0, 5) == "Grbl ")
            break;
    }

    talk("?");
    talk("G21");
    talk("G90");
    set_zero();
}
    
void cnc_machine::reset()
{
    talk("\x18");
    spindle_speed_ = 0;
    spindle_on_ = false;
    get_status();
}

void cnc_machine::get_status()
{
    std::string status;
    while (status.empty()) {
        auto st = talk("?");
        if (!st.empty())
            status = st[0];            
    }

    point wpos;
    point mpos;
    for (const std::string& field: split<std::string>(status, "|")) {
        if (field.substr(0, 5) == "WPos:") {
            wpos = point(field.substr(5));
        } else if (field.substr(0, 5) == "MPos:") {
            mpos = point(field.substr(5));
        } else if (field.substr(0, 4) == "WCO:") {
            wco_ = vector(field.substr(4));
        } else if (field.substr(0, 3) == "FS:") {
            auto fs = split<double>(field.substr(3), ",");
            spindle_on_ = (fs.size() >= 2 && fs[0] >= 1);
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

vector cnc_machine::wco()
{
    if (wco_.defined())
        return wco_;

    for (const std::string& line: talk("$#")) {
        auto fields = split<std::string>(line, ":");
        if (fields.size() >= 2 && fields[0] == "G54") {
            wco_ = vector(fields[1]);
            return wco_;
        }
    }
    throw grbl_error::protocol_violation();
}

void cnc_machine::redefine_position(point newpos)
{
    talk("G10 P0 L20 " + newpos.grbl());
    wco_ = vector();
    position_ = newpos;
}

void cnc_machine::move_xy(point p, cnc_machine::move_mode m)
{
    point c = position();
    if (m == move_mode::safe && c.z < MIN_SAFE_HEIGHT) {
        move_z(MIN_SAFE_HEIGHT);
        c.z = MIN_SAFE_HEIGHT;
    }

    talk("G0 " + point(p.x, p.y, c.z).grbl());
    position_ = point(p.x, p.y, c.z);
}

void cnc_machine::move_z(double z, cnc_machine::move_mode m)
{
    if (m == move_mode::safe)
        z = std::max(z, 0.1);
    
    talk("G0 Z" + std::to_string(z));
    position_.z = z;
}

void cnc_machine::feed_z(double z)
{
    talk("G1 Z" + std::to_string(z));
    position_.z = z;
}

double cnc_machine::probe()
{
    position_ = point();
    talk("G38.2 F15 Z-50");
    return position().z;
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

void cnc_machine::dwell(double seconds) { talk("G4 P" + std::to_string(seconds)); }


std::vector<std::string> cnc_machine::talk(const std::string& cmd)
{
    if (g_dump_wire)
        std::cerr << "\r\033[33m... send: " << cmd << "\033[0m" << std::endl;

    *s_ << cmd << std::endl;

    std::vector<std::string> resp;
    std::string line;
    while (std::getline(*s_, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.pop_back();

        if (g_dump_wire)
            std::cerr << "\r\033[32m... recv: " << line << "\033[0m" << std::endl;

        if (line == "ok") {
            return resp;
        } else if (starts_with(line, "error:")) {
            throw grbl_error(lexical_cast<int>(line.substr(6)));
        } else if (starts_with(line, "ALARM:")) {
            throw grbl_alarm(lexical_cast<int>(line.substr(6)));
        } else if (line.size() >= 2 && line[0] == '<' && line[line.size() - 1] == '>') {
            resp.push_back(line.substr(1, line.size() - 2));
        } else if (line.size() >= 2 && line[0] == '[' && line[line.size() - 1] == ']') {
            resp.push_back(line.substr(1, line.size() - 2));
        } else {
            continue;
        }
    }

    throw grbl_error::protocol_violation();
}



