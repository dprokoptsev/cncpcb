#pragma once

#include "gcode.h"
#include "geom.h"

namespace shapes {

gcode circle(double radius);
gcode filled_circle(double r1, double r2, double width);

gcode box(double width, double height);
gcode box(double width, double height, double radius);
gcode filled_box(double width, double height, double tool_width);

} // namespace shapes
