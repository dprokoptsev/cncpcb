#pragma once

#include "geom.h"
#include "utility.h"
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <cmath>


class cnc_machine;

class gcmd {
public:
    explicit gcmd(const point& last_point, const std::string& str);
    
    const std::string& cmd() const { return cmd_; }
    
    char letter() const { return cmd_[0]; }
    std::string str_arg() const { return cmd_.substr(1); }
    int arg() const { return arg_; }
    bool equals(char letter, int arg) const { return this->letter() == letter && this->arg() == arg; }
    
    const std::map<char, double>& tail() const { return tail_; }
    
    const ::point& point() const { return pt_; }
    void set_point(const ::point& pt);    
    
    friend std::ostream& operator << (std::ostream& s, const gcmd& c);
    
    template<class F>
    gcmd xform_by(const F& xf) const
    {
        gcmd ret(*this);
        if (pt_.defined())
            ret.pt_ = xf(pt_);
        return ret;
    }
    
private:
    std::string cmd_;
    std::map<char, double> tail_;
    ::point pt_;
    int arg_ = 0;
};


class gcode {
public:
    gcode();
    explicit gcode(std::istream&);
    
    std::vector<gcmd>::const_iterator begin() const { return cmds_.begin(); }
    std::vector<gcmd>::const_iterator end() const { return cmds_.end(); }
    size_t size() const { return cmds_.size(); }
    const gcmd& operator[](size_t idx) const { return cmds_[idx]; }
    
    template<class F>
    void xform_by(const F& xf)
    {
        for (gcmd& cmd: cmds_)
            cmd = cmd.xform_by(xf);
    }
    
    void break_long_legs();
    
    void send_to(cnc_machine& cnc, const std::string& prompt = std::string());
    
    ::bounding_box bounding_box() const;
    
private:
    enum gcmd_classification { USE, IGNORE, ERROR };

    gcmd_classification classify(const gcmd& c);
    
private:
    std::vector<gcmd> cmds_;
    ssize_t resume_point_ = 0;
};
