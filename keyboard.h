#pragma once

#include "geom.h"
#include "cnc.h"
#include <termios.h>
#include <string>
#include <vector>


enum class key {
    eof,
    up,
    down, 
    left,
    right,
    page_up,
    page_down,
    plus,
    minus,
    multiplies,
    divides,
    space,
    enter,
    q,
    p,
    y
};


class rawtty {
public:
    rawtty();
    ~rawtty();
    key getkey();
private:
    struct termios saved_;
};


namespace interactive {

void clear_line();

point position(
    cnc_machine& cnc, const std::string& prompt,
    cnc_machine::move_mode mode = cnc_machine::move_mode::safe
);

vector angle(cnc_machine& cnc, const std::string& prompt, const point& zero, const vector& orig);

void change_tool(cnc_machine& cnc, const std::string& prompt);


class point_list {
public:
    explicit point_list(cnc_machine& cnc, std::vector<point> pts, const std::vector<std::string> desc = {}):
        cnc_(&cnc), pts_(pts), desc_(desc) {}
    explicit point_list(cnc_machine& cnc, const std::vector<std::string> desc = {}):
        cnc_(&cnc), desc_(desc) {}
    
    ssize_t show(const std::string& prompt, size_t start_idx = 0);
    bool edit(const std::string& prompt);
    void read(const std::string& prompt, size_t sz);
    
    std::vector<point>& points() { return pts_; }

private:
    cnc_machine* cnc_;
    std::vector<point> pts_;
    std::vector<std::string> desc_;
    
    double feed_xy_;
    double feed_z_;
    
    cnc_machine& cnc() { return *cnc_; }
    std::string point_desc(size_t idx) const;
};


class progress_bar {
public:
    progress_bar(const std::string& prompt, size_t max):
        prompt_(prompt), max_(max), cur_(0)
    { update(); }
    
    ~progress_bar() { clear_line(); }
    
    void set(size_t value) { cur_ = value; update(); }
    void increment() { ++cur_; update(); }
    
private:
    std::string prompt_;
    size_t max_;
    size_t cur_;
    
    void update();
};

} // namespace interactive
