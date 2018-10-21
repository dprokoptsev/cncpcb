#include <catch.hpp>
#include "../geom.h"
#include "utility.h"

TEST_CASE("vector_basic", "[geometry][vector]")
{
    CHECK(vector(1,2,3) + vector(4,5,6) == approx(vector(5,7,9)));
    CHECK(vector(1,2,3) - vector(4,5,6) == approx(vector(-3,-3,-3)));
    CHECK(vector(1,2,3) * 2 == approx(vector(2,4,6)));
    CHECK(vector(1,2,3) / 2 == approx(vector(0.5,1,1.5)));
    
    CHECK(vector(1,2,3) * vector(2,-4,6) == approx(2-8+18));
    CHECK(vector(3,0,4).length() == approx(5));
    CHECK(vector(10,10,0).unit() == approx(vector(sqrt(2)/2, sqrt(2)/2, 0)));    
}

TEST_CASE("rotations", "[geometry][vector]")
{
    auto axis = vector::axis();
    CHECK(axis.x().rotate(deg(30)) == approx(vector(sqrt(3)/2, 0.5, 0)));
    CHECK(axis.x().rotate(deg(90)) == approx(axis.y()));
    CHECK(axis.x().rotate(deg(30)).rotate(deg(15)) == approx(vector(sqrt(2)/2, sqrt(2)/2, 0)));

    CHECK(vector(sqrt(3), 1, 0).rotate(vector(sqrt(3)/2, 0.5, 0)) == approx(vector(1, sqrt(3), 0)));
    
    CHECK(axis.x().angle_to({sqrt(3), 1, 0}) == approx(deg(30)));
    CHECK(vector(sqrt(3), 1, 0).angle_to(axis.x()) == approx(deg(-30)));
    CHECK(axis.x().angle_to(axis.y()) == approx(deg(90)));
    CHECK(fabs(vector(1,2,0).angle_to({-1,-2,0})) == approx(deg(180)));
}

TEST_CASE("projections", "[geometry][vector]")
{
    CHECK(vector(1, sqrt(3), 0).project(vector(sqrt(3)/2, 0.5, 0)) == approx(vector(1.5, sqrt(3)/2, 0)));
}

TEST_CASE("orientation", "[geometry][xform]")
{
    orientation o({ -44.5, -0.08, 0 }, { 9.38, -3.12, 0 }, { 1,0,0 });
    CHECK(o({ -47.5, -0.08, 0 }) == approx(point(6.38, -3.12, 0)));
    
    orientation r = orientation::reconstruct({
        { 11, 5, 0 },
        { 10, 6, 0 },
        {  9, 5, 0 },
        { 10, 4, 0 }
    },{
        { -sqrt(2)/2, sqrt(2)/2, 0 },
        { -sqrt(2)/2, -sqrt(2)/2, 0 },
        { sqrt(2)/2, -sqrt(2)/2, 0 },
        { sqrt(2)/2, sqrt(2)/2, 0 }
    });
    
    CHECK(r({10, 5, 0}) == approx(point(0, 0, 0)));
    CHECK(r({10+sqrt(2), 5+sqrt(2), 0}) == approx(point(-2, 0, 0)));
}
