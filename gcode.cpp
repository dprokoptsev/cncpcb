#include "gcode.h"
#include "geom.h"
#include "keyboard.h"


gcmd::gcmd(const ::point& last_point, const std::string& str)
{
    if (starts_with(str, "(MSG,")) {
        auto i = str.begin() + 5;
        while (i != str.end() && isspace(*i))
            ++i;
        cmd_ = "*" + std::string(i, str.end());
        return;
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

        if (cmd_.empty()) {
            std::stringstream stream;
            stream << arg;
            stream >> arg_;

            arg.insert(arg.begin(), letter);
            cmd_ = arg;
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
            tail_.insert(std::make_pair(letter, lexical_cast<double>(arg)));
        }
    }

    if (has_pt)
        pt_ = pt;
}

void gcmd::set_point(const ::point& pt)
{
    if (pt_.defined() != pt.defined())
        throw std::logic_error("trying to (un)define a point");
    pt_ = pt;
}

gcmd gcmd::xform_by(const orientation& o) const
{
    gcmd ret(*this);
    if (pt_.defined())
        ret.pt_ = o(pt_);
    return ret;
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

        gcmd c(last_pt, line);
        if (c.point().defined())
            last_pt = c.point();

        auto ct = classify(c);
        if (ct == USE)
            cmds_.push_back(c);
        else if (ct == ERROR)
            throw std::runtime_error("unknown command: " + lexical_cast<std::string>(c));
    }
}


std::pair<point, point> gcode::bounding_box() const
{
    point min, max;
    min.z = max.z = 0;
    for (const gcmd& c: *this) {
        point pt = c.point();
        if (!pt.defined())
            continue;

        if (std::isnan(min.x) || min.x > pt.x)
            min.x = pt.x;
        if (std::isnan(min.y) || min.y > pt.y)
            min.y = pt.y;
        if (std::isnan(max.x) || max.x < pt.x)
            max.x = pt.x;
        if (std::isnan(max.y) || max.y < pt.y)
            max.y = pt.y;
    }

    return { min, max };
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

void gcode::xform_by(const orientation& o)
{
    for (gcmd& cmd: cmds_)
        cmd = cmd.xform_by(o);
}

void gcode::send_to(cnc_machine& cnc, const std::string& prompt)
{
    bool in_tool_chg = false;
    std::string tool_chg_prompt;
    
    interactive::progress_bar progress(prompt.empty() ? "Working" : prompt, cmds_.size());
    for (const gcmd& cmd: cmds_) {
        
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
                interactive::position(cnc, "Program paused");
            }
            
        } else {
            cnc.send_gcmd(cmd);
        }

        progress.increment();
    }
    
    std::cerr << std::endl;
}
