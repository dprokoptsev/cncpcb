#pragma once

#include "geom.h"
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


class cnc_machine;


namespace interactive {

point position(cnc_machine& cnc, const std::string& prompt);
vector angle(cnc_machine& cnc, const std::string& prompt, const point& zero, const vector& orig);
ssize_t show_point_list(cnc_machine& cnc, const std::string& prompt, const std::vector<point>& pts, size_t start_idx = 0);
void edit_point_list(cnc_machine& cnc, const std::string& prompt, std::vector<point>& pts);
void change_tool(cnc_machine& cnc, const std::string& prompt);

} // namespace interactive
