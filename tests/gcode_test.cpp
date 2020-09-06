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
G02 X30 Y40 I0 J10
M5
)");
    gcode g(gfile);
    
    CHECK(g.size() == 12);
    CHECK(!g[0].point().defined());
    CHECK(g[1].point() == approx(point(10, 10, 1)));
    CHECK(g[2].point() == approx(point(20, 10, 1)));
    CHECK(!g[3].point().defined());
    CHECK(!g[4].point().defined());
    CHECK(g[6].point() == approx(point(20, 10, -1.7)));
    CHECK(g[7].point() == approx(point(20, 10, 1)));
    CHECK(g[8].point() == approx(point(20, 30, 1)));
    CHECK(g[10].point() == approx(point(30, 40, -1.7)));
    CHECK(g[10].delta() == approx(vector(0, 10, NAN)));
}

TEST_CASE("gcode_serialize", "[gcode]")
{
    gcmd g = gcmd::parse(point(), "G03 X10 Y20 I30 J40");
    CHECK(lexical_cast<std::string>(g) == "G03 X10.000 Y20.000 I30.000 J40.000");
}

TEST_CASE("gcode_orient", "[gcode][xform]")
{
    gcmd g = gcmd::parse(point(), "G00 X10Y0Z0 F1");
    orientation o({5,0,0}, {0,0,0}, vector::axis::x().rotate(deg(30)));
    
    gcmd gx = g.xform_by(o);
    CHECK(gx.point() == approx(point(2.5*sqrt(3), 2.5, 0)));
    CHECK(lexical_cast<std::string>(gx) == "G00 X4.330 Y2.500 Z0.000 F1.000");
    
    gcmd g2 = gcmd::parse(g.point(), "G03 X15 Y5 I0 J5");
    gcmd gx2 = g2.xform_by(o);
    CHECK(gx2.point() == approx(point(2.5*(2*sqrt(3)-1), 2.5*(sqrt(3)+2), 0)));
    CHECK(gx2.delta() == approx(vector(-2.5, 2.5*sqrt(3), NAN)));
}

TEST_CASE("gcode_long_legs", "[gcode]")
{
    std::istringstream gfile(R"(
S1000 F75
M3
G0 X0Y0Z0
G4
G0 X10Y0
G1 X0Y10
G0 X10Y10
G1 X0Y0
M5
)");
    gcode g(gfile);
    g.break_long_legs();
    
    REQUIRE(g.size() == 9+14);
    
    auto i = g.begin(), ie = g.end();
    CHECK((i++)->letter() == 'S');
    CHECK((i++)->equals('M', 3));
    CHECK(i->equals('G', 0)); CHECK((i++)->point() == approx(point(0, 0, 0)));
    CHECK((i++)->equals('G', 4));
    CHECK(i->equals('G', 0)); CHECK((i++)->point() == approx(point(10, 0, 0)));
    
    for (int x = 1; x != 8; ++x) {
        CHECK(i->equals('G', 1));
        CHECK((i++)->point() == approx(point(10 - x*sqrt(2), x*sqrt(2), 0)));
    }    
    CHECK(i->equals('G', 1)); CHECK((i++)->point() == approx(point(0, 10, 0)));
    
    CHECK(i->equals('G', 0)); CHECK((i++)->point() == approx(point(10, 10, 0)));

    for (int x = 1; x != 8; ++x) {
        CHECK(i->equals('G', 1));
        CHECK((i++)->point() == approx(point(10 - x*sqrt(2), 10 - x*sqrt(2), 0)));
    }
    CHECK(i->equals('G', 1)); CHECK((i++)->point() == approx(point(0, 0, 0)));
    CHECK((i++)->equals('M', 5));
    
    CHECK(i == ie);
}
