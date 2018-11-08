#define IN_TESTS

#include "utility.h"
#include <catch.hpp>
#include <random>
#include "../workflow.h"

static const char* BORDER = R"(
M3
G0 X0 Y0
G1 Z-1
G1 X10 Y0
G1 X10 Y10
G1 X0 Y10
G1 X0 Y0
G0 Z2
M5
)";

std::unique_ptr<gcode> parse(const char* str)
{
    std::istringstream s(str);
    return std::make_unique<gcode>(s);
}

TEST_CASE("extract_drills", "[workflow]")
{
    static const char* DRILL = R"(
T1
M5
M6
(MSG, Change to tool dia=0.4)
M0
M3

G0 X2 Y2 Z1
G1 Z-1.8
G1 Z0
G0 Z1
G0 X4 Y2
G1 Z-1.8
G1 Z0
G0 Z1

T1
M5
M6
(MSG, Change to tool dia=0.8)
M0
M3

G0 X6 Y2 Z1
G1 Z-1.8
G1 Z0
G0 Z1
G0 X8 Y2
G1 Z-1.8
G1 Z0
G0 Z1
)";
    
    workflow w;
    w.set_border(parse(BORDER));
    w.set_drill(parse(DRILL));
    
    std::vector<circular_area> drills = w.drills();
    CHECK(drills.size() >= 4);
    
    CHECK(drills[0].center == approx(point(2, 2, 0)));
    CHECK(drills[0].radius == approx(0.2));
    CHECK(drills[1].center == approx(point(4, 2, 0)));
    CHECK(drills[1].radius == approx(0.2));
    CHECK(drills[2].center == approx(point(6, 2, 0)));
    CHECK(drills[2].radius == approx(0.4));
    CHECK(drills[3].center == approx(point(8, 2, 0)));
    CHECK(drills[3].radius == approx(0.4));
}
