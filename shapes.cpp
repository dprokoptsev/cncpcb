#include "gcode.h"
#include "geom.h"
#include <vector>
#include <stdexcept>

namespace shapes {

namespace impl {

std::vector<gcmd> circle(double radius)
{
    static const double PRECISION = 0.01 /*mm*/;
    std::vector<gcmd> ret;
    
    double step = PRECISION / radius;
    for (double a = 0; a < 2*M_PI; a += step)
        ret.push_back(gcmd("G1", point(0,0,0) + vector::axis::x(radius).rotate(a)));
    
    return ret;
}

} // namespace impl

gcode circle(double radius)
{
    std::vector<gcmd> ret = impl::circle(radius);
    ret.insert(ret.begin(), gcmd("G1", ret.front().point()));
    return gcode(ret.begin(), ret.end());
}

gcode filled_circle(double radius, double width)
{
    if (width/2 > radius)
        throw std::runtime_error("width exceeds diameter");

    std::vector<gcmd> ret;
    
    auto add_circle = [&ret](double r) {
        std::vector<gcmd> c = impl::circle(r);
        ret.push_back(gcmd("G1", c.front().point()));
        ret.insert(ret.end(), c.begin(), c.end());
    };
    
    for (double r = width/3; r < radius - width/2; r += width/2)
        add_circle(r);

    add_circle(radius - width/2);    
    
    return gcode(ret.begin(), ret.end());
}

} // namespace shapes
