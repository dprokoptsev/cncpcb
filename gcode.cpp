#include "gcode.h"
#include "geom.h"
#include "keyboard.h"


gcmd gcmd::parse(const ::point& last_point, const std::string& str)
{
    gcmd ret;
    
    if (starts_with(str, "(MSG,")) {
        auto i = str.begin() + 5;
        while (i != str.end() && isspace(*i))
            ++i;
        ret.cmd_ = "*" + std::string(i, str.end());
        return ret;
    }

    auto expect = [&str](bool b) {
        if (!b)
            throw std::runtime_error("bad gcommand: " + str);
    };

    std::string::const_iterator i = str.begin(), ie = str.end();

    bool has_pt = false;
    ::point pt = last_point;

    while (i != ie) {
        char letter = 0;
        std::string arg;

        for (; i != ie && isspace(*i); ++i) {}
        if (i == ie)
            break;

        expect(isalpha(letter = *i));
        expect(++i != ie);
        while (i != ie && (isdigit(*i) || *i == '.' || *i == '-'))
            arg.push_back(*i++);

        if (ret.cmd_.empty()) {
            std::stringstream stream;
            stream << arg;
            stream >> ret.arg_;

            arg.insert(arg.begin(), letter);
            ret.cmd_ = arg;
        } else if (letter == 'X') {
            pt.x = lexical_cast<double>(arg);
            has_pt = true;
        } else if (letter == 'Y') {
            pt.y = lexical_cast<double>(arg);
            has_pt = true;
        } else if (letter == 'Z') {
            pt.z = lexical_cast<double>(arg);
            has_pt = true;
        } else {
            ret.tail_.insert(std::make_pair(letter, lexical_cast<double>(arg)));
        }
    }

    if (has_pt)
        ret.pt_ = pt;

    return ret;
}

void gcmd::set_point(const ::point& pt)
{
    if (pt_.defined() != pt.defined())
        throw std::logic_error("trying to (un)define a point");
    pt_ = pt;
}

std::ostream& operator << (std::ostream& s, const gcmd& c)
{
    s << c.cmd_;
    if (c.pt_.defined())
        s << " " << c.pt_.grbl();
    for (const auto& kv: c.tail_)
        s << " " << kv.first << std::fixed << std::setprecision(3) << kv.second;
    return s;
}


gcode::gcode(std::istream& s)
{
    point last_pt { 0, 0, 0 };

    std::string line;
    while (std::getline(s, line)) {
        if (line.empty())
            continue;

        gcmd c = gcmd::parse(last_pt, line);
        if (c.point().defined())
            last_pt = c.point();

        auto ct = classify(c);
        if (ct == USE)
            cmds_.push_back(c);
        else if (ct == ERROR)
            throw std::runtime_error("unknown command: " + lexical_cast<std::string>(c));
    }
}

void gcode::break_long_legs()
{
    double MAX_LEG_LENGTH = 2.0 /*mm*/;

    std::vector<gcmd> newcmds;    
    auto i = cmds_.begin(), ie = cmds_.end();
    
    for (; i != ie && !i->point().defined(); ++i)
        newcmds.push_back(*i);

    if (i == ie)
        return;
    point current_position = i->point();
    newcmds.push_back(*i++);
    
    for (; i != ie; ++i) {
        if (i->equals('G', 1)) {
            vector v = i->point() - current_position;
            for (size_t step = 1; v.length() > step * MAX_LEG_LENGTH; ++step) {
                gcmd intermediate(*i);
                intermediate.set_point(current_position + step*MAX_LEG_LENGTH*v.unit());
                newcmds.push_back(intermediate);
            }
        }
        
        newcmds.push_back(*i);
        if (i->point().defined())
            current_position = i->point();
    }
    
    cmds_ = std::move(newcmds);
}

::bounding_box gcode::bounding_box() const
{
    ::bounding_box box;
    for (const gcmd& c: *this) {
        point pt = c.point();
        if (pt.defined())
            box.extend(pt);
    }
    return box;
}

gcode::gcmd_classification gcode::classify(const gcmd& c)
{
    if (c.letter() == '*') {
        return USE;
    } else if (c.letter() == 'M') {
        if (c.arg() == 0 || c.arg() == 3 || c.arg() == 4 || c.arg() == 5 || c.arg() == 6)
            return USE;
    } else if (c.letter() == 'G') {
        if (
            c.arg() == 0 || c.arg() == 1 || c.arg() == 4 || c.arg() == 21
            || c.arg() == 90 || c.arg() == 94
        )
            return USE;    
    } else if (c.letter() == 'S' || c.letter() == 'F') {
        return USE;
    } else if (c.letter() == 'T') {
        return USE;
    }

    return ERROR;
}

void gcode::send_to(cnc_machine& cnc, const std::string& prompt)
{
    bool in_tool_chg = false;
    std::string tool_chg_prompt;
    
    interactive::progress_bar progress(prompt.empty() ? "Working" : prompt, cmds_.size());
    cnc.move_z(1);
    
    bool fast_forward = (resume_point_ != 0);
    auto i = cmds_.begin() + resume_point_, ie = cmds_.end();
    for (; i != ie; ++i) {
        const gcmd& cmd = *i;
        
        if (cmd.letter() == 'T' || cmd.equals('M', 6)) {
            in_tool_chg = true;
            
        } else if (cmd.letter() == '*') {
            
            if (in_tool_chg) {
                tool_chg_prompt = cmd.str_arg();
            } else {
                interactive::clear_line();
                std::cerr << "MSG: " << cmd.str_arg() << std::endl;
            }
            
        } else if (cmd.equals('M', 0)) {
            
            interactive::clear_line();
            if (in_tool_chg) {
                cnc.set_spindle_off();
                interactive::change_tool(cnc, tool_chg_prompt);
                in_tool_chg = false;
            } else {
                interactive::position(cnc, orientation::identity(), "Program paused");
            }
            
        } else {
            if (cmd.equals('G', 0) && fast_forward && std::distance(cmds_.begin(), i) == resume_point_)
                fast_forward = false;
            
            if (fast_forward && (cmd.equals('G', 1) || cmd.equals('G', 0)))
                continue;
            
            if (!fast_forward && cmd.equals('G', 0))
                resume_point_ = std::distance(cmds_.begin(), i);
                
            cnc.send_gcmd(cmd);
        }

        progress.increment();
    }
    
    std::cerr << std::endl;
}

point gcode::frontpt() const
{
    for (const gcmd& gc: *this)
        if (gc.point().defined())
            return gc.point();
    return point();
}
