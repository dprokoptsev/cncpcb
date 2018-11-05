#pragma once

#include "geom.h"
#include <vector>
#include <iostream>
#include <algorithm>
#include <random>

class height_map {
public:
    explicit height_map(const bounding_box& bbox, const std::vector<circular_area>& avoid): bbox_(bbox)
    {
        static const double SUGGESTED_CELL_SIZE = 10;
        static const double SAFETY_MARGIN = 0.2;
        cell_count_x_ = unsigned(ceil(bbox.size().x / SUGGESTED_CELL_SIZE));
        cell_count_y_ = unsigned(ceil(bbox.size().y / SUGGESTED_CELL_SIZE));
        
        std::mt19937 rand;
        rand.seed(1);
        std::uniform_real_distribution<double> rand_angle(0, 2*M_PI);
        
        for (size_t y = 0; y != cell_count_y_ + 1; ++y) {
            for (size_t x = 0; x != cell_count_x_ + 1; ++x) {
                point pt = bbox.bottom_left() + vector(x*cell_size_x(), y*cell_size_y(), 0);
                
                point shifted_pt = pt;
                for (;;) {
                    auto closest = std::min_element(
                        avoid.begin(), avoid.end(),
                        [&shifted_pt](const circular_area& a, const circular_area& b) {
                            return a.distance_to(shifted_pt) < b.distance_to(shifted_pt);
                        }
                    );
                    
                    if (closest == avoid.end() || closest->distance_to(shifted_pt) > SAFETY_MARGIN)
                        break;
                    
                    shifted_pt = pt + vector::axis::x(closest->radius + SAFETY_MARGIN + 0.05).rotate(rand_angle(rand));
                }
                
                shifted_pt.z = std::numeric_limits<double>::quiet_NaN();
                pts_.push_back(shifted_pt);
            }
        }
    }
    
    explicit height_map(std::istream& s)
    {
        if (!(s >> cell_count_x_ >> cell_count_y_))
            throw std::runtime_error("bad height map");
        pts_.resize((cell_count_x_+1) * (cell_count_y_+1));
        for (point& pt: pts_)
            s >> pt.x >> pt.y >> pt.z;
        if (!s)
            throw std::runtime_error("bad height map");
    }
    
    void save(std::ostream& s) const
    {
        s << cell_count_x_ << " " << cell_count_y_ << "\n";
        for (const point& pt: pts_)
            s << pt.x << " " << pt.y << " " << pt.z << "\n";
    }

    const ::bounding_box& bounding_box() const { return bbox_; }
    
    point& measurement(size_t x, size_t y) { return pts_[y*(cell_count_x_+1) + x]; }
    const point& measurement(size_t x, size_t y) const { return pts_[y*(cell_count_x_+1) + x]; }
    
    std::vector<point>::iterator begin() { return pts_.begin(); }
    std::vector<point>::iterator end() { return pts_.end(); }

    bool defined() const { return std::all_of(pts_.begin(), pts_.end(), [](const point& pt) { return pt.defined(); }); }

    point operator()(point pt) const
    {
        unsigned x = unsigned(floor((pt.x - bbox_.bottom_left().x) / cell_size_x()));
        unsigned y = unsigned(floor((pt.y - bbox_.bottom_left().y) / cell_size_y()));
        
        if (x == cell_count_x_)
            --x;
        if (y == cell_count_y_)
            --y;
        
        point lb = measurement(x, y);
        point lt = measurement(x, y+1);
        point rb = measurement(x+1, y);
        point rt = measurement(x+1, y+1);
        
        point l = lb + (lt - lb) * ((pt.y - lb.y) / (lt.y - lb.y));
        point r = rb + (rt - rb) * ((pt.y - rb.y) / (rt.y - rb.y));
        point m = l + (r - l) * ((pt.x - l.x) / (r.x - l.x));
        
        return { pt.x, pt.y, pt.z + m.z };
    }
    
private:
    ::bounding_box bbox_;
    unsigned cell_count_x_, cell_count_y_;
    std::vector<point> pts_;
            
    double cell_size_x() const { return bbox_.size().x / cell_count_x_; }
    double cell_size_y() const { return bbox_.size().y / cell_count_y_; }
};
