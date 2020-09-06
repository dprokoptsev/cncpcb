#pragma once

#include "geom.h"

class depth_list {
public:
    depth_list() {}
    explicit depth_list(double d): depths_({d}) {}
    depth_list(double start_depth, double end_depth, double step);

    const std::vector<double>& depths() const { return depths_; }
    polyline operator()(polyline p) const;

private:
    std::vector<double> depths_;
};

namespace shapes {

inline polyline segment(vector v) { return { {0,0,0}, point::from_vector(v) }; }

polyline circle(double radius);
polyline filled_circle(double r1, double r2, double width);

polyline box(double width, double height);
polyline box(double width, double height, double radius);
polyline filled_box(double width, double height, double tool_width);

} // namespace shapes
