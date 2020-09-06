#include "geom.h"
#include "shapes.h"
#include <vector>
#include <algorithm>
#include <stdexcept>

depth_list::depth_list(double d1, double d2, double step)
{
    size_t steps = size_t(ceil((d2 - d1) / step - 0.001));
    step = (d2 - d1) / steps;
    for (double d = d1; steps--; d += step)
        depths_.push_back(d);
    depths_.push_back(d2);
}

polyline depth_list::operator()(polyline p) const
{
    polyline ret;

    for (double depth: depths_) {
        std::transform(
            p.begin(), p.end(), std::back_inserter(ret),
            [d = vector::axis::z(-depth)](point pt) { return pt + d; }
        );
        std::reverse(p.begin(), p.end());
    }
    return ret;
}

namespace shapes {

namespace impl {

template<class OutIter>
OutIter arc(point center, double radius, double start_angle, double arc_angle, OutIter out)
{
    static const double PRECISION = 0.05 /*mm*/;
    double step = PRECISION / radius;
    double direction = arc_angle / fabs(arc_angle);
    for (double a = 0; fabs(a) < fabs(arc_angle); a += step * direction)
        *out++ = center + vector::axis::x(radius).rotate(start_angle + a);
    return out;
}

template<class OutIter>
OutIter circle(double radius, OutIter out)
{
    *out++ = point::from_vector(vector::axis::x(radius));
    out = arc(point(0,0,0), radius, 0, 2*M_PI, out);
    *out++ = point::from_vector(vector::axis::x(radius));
    return out;
}   

} // namespace impl

polyline circle(double radius)
{
    polyline ret;
    impl::circle(radius, std::back_inserter(ret));
    return ret;
}

polyline filled_circle(double r1, double r2, double width)
{
    double inner_radius = std::min(r1, r2);
    double outer_radius = std::max(r1, r2);
    if (width/2 > outer_radius - inner_radius)
        throw std::runtime_error("tool width exceeds shape width");
    
    polyline ret;
    for (double r = inner_radius + width/2; r < outer_radius - width*0.75; r += width/2)
        impl::circle(r, std::back_inserter(ret));

    impl::circle(outer_radius - width/2, std::back_inserter(ret));    
    
    return ret;
}

polyline box(double width, double height)
{
    polyline ret;
    ret.push_back(point(0,0,0));
    ret.push_back(point(width, 0, 0));
    ret.push_back(point(width, height, 0));
    ret.push_back(point(0, height, 0));
    ret.push_back(point(0,0,0));
    return ret;
}

polyline box(double width, double height, double radius)
{
    polyline ret;

    ret.push_back(point(0, radius, 0));
    
    ret.push_back(point(0, height-radius, 0));
    impl::arc(point(radius, height-radius, 0), radius, M_PI, -M_PI/2, std::back_inserter(ret));
    
    ret.push_back(point(width-radius, height, 0));
    impl::arc(point(width-radius, height-radius, 0), radius, M_PI/2, -M_PI/2, std::back_inserter(ret));
    
    ret.push_back(point(width, radius, 0));
    impl::arc(point(width-radius, radius, 0), radius, 0, -M_PI/2, std::back_inserter(ret));
    
    ret.push_back(point(radius, 0, 0));
    impl::arc(point(radius, radius, 0), radius, -M_PI/2, -M_PI/2, std::back_inserter(ret));
    
    ret.push_back(point(0, radius, 0));
    return ret;
}

polyline filled_box(double width, double height, double tool_width)
{
    polyline ret;
    double tw2 = tool_width/2;
    vector yleg = vector::axis::y(height - 4*tw2);
    point pt(2*tw2, 2*tw2, 0);
    double dir = 1;
    ret.push_back(pt);
    
    bool first = true;
    for (double x = 2*tw2; x < width - 2*tw2; x += tw2/2) {
        if (first) {
            first = false;
        } else {
            impl::arc(pt + vector::axis::x(tw2/4), tw2/4, -M_PI, M_PI * dir, std::back_inserter(ret));
            pt += vector::axis::x(tw2/2);
        }

        pt += yleg * dir;
        ret.push_back(pt);
        dir *= -1;
    }
    
    ret.push_back(point(width - tw2, tw2, 0));
    ret.push_back(point(width - tw2, height - tw2, 0));
    ret.push_back(point(tw2, height - tw2, 0));
    ret.push_back(point(tw2, tw2, 0));
    ret.push_back(point(width - tw2, tw2, 0));

    return ret;
}

} // namespace shapes
