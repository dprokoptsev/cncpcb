#include <catch.hpp>
#include <sstream>
#include "../gcode.h"
#include "utility.h"

TEST_CASE("gcode_parse", "[gcode]")
{
    std::istringstream gfile(R"(
S1000
G00 X10Y10Z1
G00 X20
G04 P1
(MSG,A message)
M3
G01 Z-1.7 F4
G01 Z1
G00 Y30
G01 Z-1.7
M5
)");
    gcode g(gfile);
    
    CHECK(g.size() == 11);
    CHECK(!g[0].point().defined());
    CHECK(g[1].point() == approx(point(10, 10, 1)));
    CHECK(g[2].point() == approx(point(20, 10, 1)));
    CHECK(!g[3].point().defined());
    CHECK(!g[4].point().defined());
    CHECK(g[6].point() == approx(point(20, 10, -1.7)));
    CHECK(g[7].point() == approx(point(20, 10, 1)));
    CHECK(g[8].point() == approx(point(20, 30, 1)));
}

TEST_CASE("gcode_orient", "[gcode][xform]")
{
    gcmd g(point(), "G00 X10Y0Z0 F1");
    orientation o({5,0,0}, {0,0,0}, vector::axis::x().rotate(deg(30)));
    
    gcmd gx = g.xform_by(o);
    CHECK(gx.point() == approx(point(5*sqrt(3)/2, 2.5, 0)));
    CHECK(lexical_cast<std::string>(gx) == "G00 X4.330 Y2.500 Z0.000 F1.000");
}
