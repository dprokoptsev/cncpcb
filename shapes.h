#pragma once

#include "gcode.h"
#include "geom.h"

namespace shapes {

gcode circle(double radius);
gcode filled_circle(double radius, double width);

} // namespace shapes
