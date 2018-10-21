#include "cnc.h"
#include "keyboard.h"
#include "workflow.h"
#include "gcode.h"
#include "settings.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cassert>

static std::vector<point> make_reference_points(const gcode& border)
{
    static const double MARGIN = 3.0;
    auto bbox = border.bounding_box();
    point min = bbox.first, max = bbox.second;
    
    std::vector<point> ret;
    ret.push_back({ min.x - MARGIN, min.y, 0 });
    ret.push_back({ min.x, max.y + MARGIN, 0 });
    ret.push_back({ max.x, max.y + MARGIN, 0 });
    ret.push_back({ max.x + MARGIN, min.y, 0 });
    
    return ret;
}


void workflow::load_border(const std::string& filename)
{
    std::ifstream f(filename);
    border_.reset(new gcode(f));
}

void workflow::set_orientation()
{
    if (!border_)
        throw error("border not loaded");

    orientation orient;    

    auto bbox = border_->bounding_box();
    vector size = bbox.second - bbox.first;
    std::cout << "Size: " << size << std::endl;
    vector v = size;

    for (;;) {
        point ll = interactive::position(cnc(), "Lower-left corner");
        ll.z = 0;
        v = interactive::angle(cnc(), "Upper-right corner", ll, v);

        orient = orientation(bbox.first, ll, vector::axis::x().rotate(size.angle_to(v)));
        std::cout << "Orientation: " << orient << std::endl;

        std::vector<point> refpts = make_reference_points(*border_);
        assert(!refpts.empty());
        std::transform(refpts.begin(), refpts.end(), refpts.begin(), orient);
        
        size_t nearest = std::distance(
            refpts.begin(), std::min_element(
                refpts.begin(), refpts.end(),
                [this](point a, point b) {
                    return cnc().position().distance_to(a) < cnc().position().distance_to(b);
                }
            )
        );
        if (interactive::show_point_list(cnc(), "Reference holes", refpts, nearest) != -1)
            break;

        cnc().move_xy(ll);
    }
    
    orient_ = std::move(orient);
}


void workflow::drill_reference_holes()
{
    if (!border_)
        throw error("border not loaded");
    
    interactive::change_tool(cnc(), "Change tool to drill ~0.5mm");
    
    std::vector<point> refpts = make_reference_points(*border_);
    std::transform(refpts.begin(), refpts.end(), refpts.begin(), orient_);
    
    const auto& settings = ::settings::DRILL;
    cnc().move_z(settings.travel_z);
    cnc().set_spindle_speed(settings.spindle_speed);
    cnc().set_feed_rate(settings.feed_rate);
    cnc().set_spindle_on();
    cnc().dwell(1);
    
    for (const point& pt: refpts) {
        cnc().move_xy(pt);
        cnc().feed_z(settings.work_z);
        cnc().feed_z(0);
        cnc().move_z(settings.travel_z);
    }
    
    cnc().set_spindle_off();
}


void workflow::use_reference_holes()
{
    if (!border_)
        throw error("border not loaded");
    
    throw error("not implemented yet");
}
