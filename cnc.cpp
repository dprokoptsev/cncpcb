#include "errors.h"
#include "utility.h"
#include "geom.h"
#include "cnc.h"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <utility>


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
}

point cnc_machine::position()
{
    if (position_.defined())
        return position_;

    std::string status;
    while (status.empty()) {
        auto st = talk("?");
        if (!st.empty())
            status = st[0];            
    }

    point mpos;
    for (const std::string& field: split<std::string>(status, "|")) {
        if (field.substr(0, 5) == "WPos:") {
            return (position_ = point(field.substr(5)));
        } else if (field.substr(0, 5) == "MPos:") {
            mpos = point(field.substr(5));
        } else if (field.substr(0, 4) == "WCO:") {
            wco_ = vector(field.substr(4));
        }
    }
    if (mpos.defined())
        return (position_ = mpos - wco_);
    else
        throw grbl_error::protocol_violation();
}

void cnc_machine::redefine_position(point newpos)
{
    talk("G10 P0 L20 " + newpos.grbl());
    wco_ = vector();
    position_ = newpos;
}

void cnc_machine::move_xy(point p)
{
    point c = position();
    if (c.z < MIN_SAFE_HEIGHT) {
        move_z(MIN_SAFE_HEIGHT);
        c.z = MIN_SAFE_HEIGHT;
    }

    talk("G0 " + point(p.x, p.y, c.z).grbl());
    position_ = point(p.x, p.y, c.z);
}

void cnc_machine::move_z(double z)
{
    talk("G0 Z" + std::to_string(z));
    position_.z = z;
}

double cnc_machine::probe()
{
    position_ = point();
    talk("G38.2 F15 Z-50");
    return position().z;
}

std::vector<std::string> cnc_machine::talk(const std::string& cmd)
{
    std::cerr << "\033[33m... send: " << cmd << "\033[0m" << std::endl;

    *s_ << cmd << std::endl;

    std::vector<std::string> resp;
    std::string line;
    while (std::getline(*s_, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.pop_back();

        std::cerr << "\033[32m... recv: " << line << "\033[0m" << std::endl;

        if (line == "ok") {
            return resp;
        } else if (line.substr(0, 6) == "error:") {
            throw grbl_error(lexical_cast<int>(line.substr(6)));
        } else if (line.substr(0, 6) == "ALARM:") {
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



