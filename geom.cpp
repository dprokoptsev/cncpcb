#include "geom.h"
#include <algorithm>
#include <numeric>

orientation orientation::reconstruct(const std::vector<point>& orig, const std::vector<point>& xformed)
{
    if (orig.size() != xformed.size())
        throw std::logic_error("orientation::reconstruct(): vector size mismatch");
    if (orig.empty() || xformed.empty())
        throw std::logic_error("orientation::reconstruct(): vector empty");

    auto center = [](const std::vector<point>& pts){
        return point::from_vector(std::accumulate(
            pts.begin(), pts.end(), vector::zero(),
            [](const vector& a, const point& pt) { return a + pt.to_vector(); }
        ) / pts.size());
    };

    orientation ret;
    ret.gcode_zero_ = center(orig);
    ret.cnc_zero_ = center(xformed);
    
    double angle = 0;
    for (size_t i = 0; i != orig.size(); ++i)
        angle += (orig[i] - ret.gcode_zero_).angle_to(xformed[i] - ret.cnc_zero_);
    
    ret.rotation_ = vector::axis::x().rotate(angle / orig.size());
    
    return ret;
}
