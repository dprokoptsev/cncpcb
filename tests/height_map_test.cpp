#define IN_TESTS

#include "utility.h"
#include <catch.hpp>
#include <random>
#include "../height_map.h"

template<class F>
void init_height_map(height_map& h, F f)
{
    for (point& pt: h)
        pt.z = f(pt);
}

TEST_CASE("hmap_grid", "[hmap]")
{
    bounding_box box({0,0,0}, {29,37,0});
    height_map h(box, {});
    init_height_map(h, [](const auto&){ return 0; });
    
    CHECK(std::distance(h.begin(), h.end()) == 20);
    CHECK(std::all_of(h.begin(), h.end(), [&box](point pt) { return box.contains(pt); }));
    
    CHECK((h.begin()+1)->distance_to(*(h.begin()+2)) == approx(29.0/3));
}

TEST_CASE("hmap_grid_holes", "[hmap]")
{
    bounding_box box({0,0,0}, {19,19,0});
    static const point center(9.5, 9.5, 0);
    
    height_map h1(box, {});
    init_height_map(h1, [](const auto&){ return 0; });
    CHECK((h1.begin() + 4)->distance_to(center) == approx(0));
    
    height_map h3(box, {{{9.5, 9.5, 0}, 0.5}});
    init_height_map(h3, [](const auto&){ return 0; });
    CHECK((h3.begin() + 4)->distance_to(center) == approx(0.75));

    height_map h4(box, {{{9.5, 9.5, 0}, 3}});
    init_height_map(h4, [](const auto&){ return 0; });
    CHECK((h4.begin() + 4)->distance_to(center) == approx(3.25));
}

TEST_CASE("hmap_interpolation", "[hmap]")
{
    auto surface = [](const point& pt) { return pt.x*0.1 + pt.y*0.3; };
    bounding_box box({ -10, -10, 0 }, { 10, 10, 0 });
    height_map h(box, {});
    init_height_map(h, surface);

    std::mt19937 rand;
    rand.seed(1);
    std::uniform_real_distribution<double> gx(box.bottom_left().x, box.top_right().x);
    std::uniform_real_distribution<double> gy(box.bottom_left().y, box.top_right().y);
    
    for (size_t i = 0; i != 10000; ++i) {
        point pt(gx(rand), gy(rand), 0);
        point ipt = h(pt);
        
        CHECK(ipt.x == approx(pt.x));
        CHECK(ipt.y == approx(pt.y));
        CHECK(ipt.z == approx(surface(ipt)));
    }
}
