#include "gcode.h"
#include "geom.h"
#include <vector>
#include <stdexcept>

namespace shapes {

namespace impl {

template<class OutIter>
OutIter arc(point center, double radius, double start_angle, double arc_angle, OutIter out)
{
    static const double PRECISION = 0.01 /*mm*/;
    double step = PRECISION / radius;
    double direction = arc_angle / fabs(arc_angle);
    for (double a = 0; fabs(a) < fabs(arc_angle); a += step * direction)
        *out++ = gcmd("G1", center + vector::axis::x(radius).rotate(start_angle + a));
    return out;
}

template<class OutIter>
OutIter circle(double radius, OutIter out)
{
    *out++ = gcmd("G1", point::from_vector(vector::axis::x(radius)));
    out = arc(point(0,0,0), radius, 0, 2*M_PI, out);
    *out++ = gcmd("G1", point::from_vector(vector::axis::x(radius)));
    return out;
}   

} // namespace impl

gcode circle(double radius)
{
    std::vector<gcmd> ret;
    impl::circle(radius, std::back_inserter(ret));
    return gcode(ret.begin(), ret.end());
}

gcode filled_circle(double r1, double r2, double width)
{
    double inner_radius = std::min(r1, r2);
    double outer_radius = std::max(r1, r2);
    if (width/2 > outer_radius - inner_radius)
        throw std::runtime_error("tool width exceeds shape width");
    
    std::vector<gcmd> ret;   
    for (double r = inner_radius + width/2; r < outer_radius - width*0.75; r += width/2)
        impl::circle(r, std::back_inserter(ret));

    impl::circle(outer_radius - width/2, std::back_inserter(ret));    
    
    return gcode(ret.begin(), ret.end());
}

gcode box(double width, double height)
{
    std::vector<gcmd> ret;
    ret.push_back(gcmd("G1", point(0,0,0)));
    ret.push_back(gcmd("G1", point(width, 0, 0)));
    ret.push_back(gcmd("G1", point(width, height, 0)));
    ret.push_back(gcmd("G1", point(0, height, 0)));
    ret.push_back(gcmd("G1", point(0,0,0)));
    return gcode(ret.begin(), ret.end());
}

gcode box(double width, double height, double radius)
{
    std::vector<gcmd> ret;

    ret.push_back(gcmd("G1", point(0, radius, 0)));
    
    ret.push_back(gcmd("G1", point(0, height-radius, 0)));
    impl::arc(point(radius, height-radius, 0), radius, M_PI, -M_PI/2, std::back_inserter(ret));
    
    ret.push_back(gcmd("G1", point(width-radius, height, 0)));
    impl::arc(point(width-radius, height-radius, 0), radius, M_PI/2, -M_PI/2, std::back_inserter(ret));
    
    ret.push_back(gcmd("G1", point(width, radius, 0)));
    impl::arc(point(width-radius, radius, 0), radius, 0, -M_PI/2, std::back_inserter(ret));
    
    ret.push_back(gcmd("G1", point(radius, 0, 0)));
    impl::arc(point(radius, radius, 0), radius, -M_PI/2, -M_PI/2, std::back_inserter(ret));
    
    ret.push_back(gcmd("G1", point(0, radius, 0)));
    return gcode(ret.begin(), ret.end());
}

gcode filled_box(double width, double height, double tool_width)
{
    std::vector<gcmd> ret;
    double tw2 = tool_width/2;
    vector ydir = vector::axis::y(height - 4*tw2);
    point pt(2*tw2, 2*tw2, 0);
    ret.push_back(gcmd("G1", pt));
    
    auto move_by = [&ret, &pt](vector v) {
        pt += v;
        ret.push_back(gcmd("G1", pt));
    };
    for (double x = 2*tw2; x < width - 2*tw2; x += tw2) {
        move_by(ydir);
        move_by(vector::axis::x(tw2));
        ydir *= -1;
    }
    
    ret.push_back(gcmd("G1", point(width - tw2, tw2, 0)));
    ret.push_back(gcmd("G1", point(width - tw2, height - tw2, 0)));
    ret.push_back(gcmd("G1", point(tw2, height - tw2, 0)));
    ret.push_back(gcmd("G1", point(tw2, tw2, 0)));
    ret.push_back(gcmd("G1", point(width - tw2, tw2, 0)));

    return gcode(ret.begin(), ret.end());
}

} // namespace shapes
