#define IN_TESTS

#include <catch.hpp>
#include "utility.h"
#include "../shapes.h"

TEST_CASE("depth_list_test", "[shapes]")
{
    CHECK(depth_list(1).depths() == approx(std::vector<double>{ 1 }));
    CHECK(depth_list(0.5, 2, 0.5).depths() == approx(std::vector<double>{ 0.5, 1, 1.5, 2 }));
    CHECK(depth_list(0.5, 2, 0.7).depths() == approx(std::vector<double>{ 0.5, 1, 1.5, 2 }));
    CHECK(depth_list(0.5, 2.5, 0.5).depths() == approx(std::vector<double>{ 0.5, 1, 1.5, 2, 2.5 }));
    CHECK(depth_list(0.5, 2.5, 0.4).depths() == approx(std::vector<double>{ 0.5, 0.9, 1.3, 1.7, 2.1, 2.5 }));
}


