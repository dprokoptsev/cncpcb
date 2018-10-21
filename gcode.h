#pragma once

#include "geom.h"
#include "utility.h"
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <cmath>


class gcmd {
public:
    explicit gcmd(const point& last_point, const std::string& str)
    {
        if (starts_with(str, "(MSG,")) {
            cmd_ = "*" + str.substr(5, str.size()-6);
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
    
    const std::string& cmd() const { return cmd_; }
    
    char letter() const { return cmd_[0]; }
    int arg() const { return arg_; }
    
    const std::map<char, double>& tail() const { return tail_; }
    const ::point& point() const { return pt_; }
    
    void set_point(const ::point& pt)
    {
        if (pt_.defined() != pt.defined())
            throw std::logic_error("trying to (un)define a point");
        pt_ = pt;
    }
    
    friend std::ostream& operator << (std::ostream& s, const gcmd& c)
    {
        s << c.cmd_;
        if (c.pt_.defined())
            s << " " << c.pt_.grbl();
        for (const auto& kv: c.tail_)
            s << " " << kv.first << std::fixed << std::setprecision(3) << kv.second;
        return s;
    }
    
private:
    std::string cmd_;
    std::map<char, double> tail_;
    ::point pt_;
    int arg_;
};


class gcode {
public:
    gcode() {}

    explicit gcode(std::istream& s)
    {
        point last_pt { 0, 0, 0 };
        
        std::string line;
        while (std::getline(s, line)) {
            if (line.empty())
                continue;
                        
            gcmd c(last_pt, line);
            
            std::cerr << "... " << line << " => " << c << std::endl;
            
            if (c.point().defined())
                last_pt = c.point();
            
            auto ct = classify(c);
            if (ct == USE)
                cmds_.push_back(c);
            else if (ct == ERROR)
                throw std::runtime_error("unknown command: " + lexical_cast<std::string>(c));
        }
    }
        
    std::vector<gcmd>::const_iterator begin() const { return cmds_.begin(); }
    std::vector<gcmd>::const_iterator end() const { return cmds_.end(); }
    
    std::pair<point, point> bounding_box() const
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
    
private:

    enum gcmd_classification {
        USE,
        IGNORE,
        ERROR
    };
    
    gcmd_classification classify(const gcmd& c)
    {
        if (c.letter() == '*')
            return USE;
        
        if (c.letter() == 'M') {
            if (c.arg() == 0 || c.arg() == 3 || c.arg() == 4 || c.arg() == 5)
                return USE;
            else if (c.arg() == 6)
                return IGNORE;
        } else if (c.letter() == 'G') {
            if (
                c.arg() == 0 || c.arg() == 1 || c.arg() == 4 || c.arg() == 21
                || c.arg() == 90 || c.arg() == 94
            )
                return USE;    
        } else if (c.letter() == 'S' || c.letter() == 'F') {
            return USE;
        }
        
        return ERROR;
    }
    
private:
    std::vector<gcmd> cmds_;
};
