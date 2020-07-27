#pragma once

#include "utility.h"
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <limits>


class point;
class vector;

class pt_base {
public:
    double x, y, z;
    
public:
    pt_base():
        x(std::numeric_limits<double>::quiet_NaN()),
        y(std::numeric_limits<double>::quiet_NaN()),
        z(std::numeric_limits<double>::quiet_NaN())
    {}
    
    pt_base(double ax, double ay, double az): x(ax), y(ay), z(az) {}
    
    explicit pt_base(const std::string& s)
    {
        auto v = split<double>(s, ",");
        if (v.size() != 3)
            throw std::bad_cast();

        x = v[0];
        y = v[1];
        z = v[2];
    }
    
    bool defined() const { return !std::isnan(x) && !std::isnan(y) && !std::isnan(z); }
    bool any_defined() const { return !std::isnan(x) || !std::isnan(y) || !std::isnan(z); }
    
    friend std::istream& operator >> (std::istream& s, pt_base& p)
    {
        s >> p.x;
        if (s.get() != ',')
            s.setstate(std::ios::badbit);
        s >> p.y;        
        if (s.get() != ',')
            s.setstate(std::ios::badbit);
        s >> p.z;
        return s;
    }
    
    friend std::ostream& operator << (std::ostream& s, const pt_base& p)
    {
        return s << '('
                 << std::setprecision(3) << p.x << ','
                 << std::setprecision(3) << p.y << ','
                 << std::setprecision(3) << p.z << ')';
    }
    
    std::string grbl() const
    {
        std::ostringstream s;
        s << std::fixed
          << "X" << std::setprecision(3) << x
          << " Y" << std::setprecision(3) << y
          << " Z" << std::setprecision(3) << z;
        return s.str();
    }
};


class vector: public pt_base {
public:
    using pt_base::pt_base;
    
    struct axis {
        static vector x(double x = 1) { return { x, 0, 0 }; }
        static vector y(double y = 1) { return { 0, y, 0 }; }
        static vector z(double z = 1) { return { 0, 0, z }; }
    };
    static vector zero() { return { 0, 0, 0 }; }
    
    vector operator + (const vector& v) const { return { x + v.x, y + v.y, z + v.z }; }
    vector operator - (const vector& v) const { return { x - v.x, y - v.y, z - v.z }; }
    vector operator * (double k) const { return { x*k, y*k, z*k }; }
    vector operator / (double k) const { return { x/k, y/k, z/k }; }
    friend vector operator * (double k, const vector& v) { return v*k; }

    vector operator-() const { return { -x, -y, -z }; }
    
    vector& operator += (const vector& v) { return (*this = *this + v); }
    vector& operator -= (const vector& v) { return (*this = *this - v); }
    vector& operator *= (double k) { return (*this = *this * k); }
    vector& operator /= (double k) { return (*this = *this / k); }
    
    double operator * (const vector& v) const { return x*v.x + y*v.y + z*v.z; }
    double length() const { return sqrt(x*x + y*y + z*z); }
    vector unit() const { return *this / length(); }
    
    vector rotate(double angle) const { return { x*cos(angle) - y*sin(angle), y*cos(angle)+x*sin(angle), z }; }
    vector rotate(vector angle) const { return { x*angle.x - y*angle.y, y*angle.x + x*angle.y, z }; }
    double angle_to(vector v) const { return atan2(v.unit() * rotate(M_PI/2).unit(), v.unit() * unit()); }
    
    vector project(const vector& axis) const { return axis * (*this * axis.unit()); }
    
    vector project_xy() const { return { x, y, 0 }; }
    vector mirror_x() const { return { -x, y, z }; }
    vector mirror_y() const { return { x, -y, z }; }
};


class point: public pt_base {
public:
    using pt_base::pt_base;
    
    point operator + (const vector& v) const { return { x + v.x, y + v.y, z + v.z }; }
    point operator - (const vector& v) const { return { x - v.x, y - v.y, z - v.z }; }
    
    point& operator += (const vector& v) { x += v.x; y += v.y; z += v.z; return *this; }
    point& operator -= (const vector& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    
    friend point operator + (const vector& v, const point& pt) { return { v.x + pt.x, v.y + pt.y, v.z + pt.z }; }
    vector operator - (const point& p) const { return { x - p.x, y - p.y, z - p.z }; }
    
    double distance_to(const point& p) const { return (p - *this).length(); }
    
    point rotate(const point& center, double angle) const { return center + (*this - center).rotate(angle); }
    
    vector to_vector() const { return *this - point(0, 0, 0); }
    static point from_vector(const vector& v) { return point(0, 0, 0) + v; }

    point project_xy() const { return { x, y, 0 }; }
};


class bounding_box {
public:
    bounding_box() { min_.z = max_.z = 0; }
    
    bounding_box(const point& a, const point& b): bounding_box() { extend(a); extend(b); }
    
    void extend(const point& pt)
    {
        if (std::isnan(min_.x) || min_.x > pt.x)
            min_.x = pt.x;
        if (std::isnan(min_.y) || min_.y > pt.y)
            min_.y = pt.y;
        if (std::isnan(max_.x) || max_.x < pt.x)
            max_.x = pt.x;
        if (std::isnan(max_.y) || max_.y < pt.y)
            max_.y = pt.y;
    }
    
    point bottom_left() const { return min_; }
    point bottom_right() const { return { max_.x, min_.y, 0 }; }
    point top_left() const { return { min_.x, max_.y, 0 }; }
    point top_right() const { return max_; }
    
    point center() const { return min_ + (max_ - min_) / 2; }
    
    vector size() const { return max_ - min_; }
    
    bool contains(const point& pt) const
    {
        return pt.x >= min_.x && pt.y >= min_.y
            && pt.x <= max_.x && pt.y <= max_.y;
    }
    
    bool contains(const bounding_box& other) const { return contains(other.min_) && contains(other.max_); }

private:
    point min_;
    point max_;
};

class circular_area {
public:
    point center;
    double radius;
    
    double distance_to(const point& pt) const { return std::max(pt.distance_to(center) - radius, 0.0); }
};


class orientation {
public:
    orientation() {}
    orientation(point gcode_zero, point cnc_zero, vector rotation):
        gcode_zero_(gcode_zero), cnc_zero_(cnc_zero), rotation_(rotation.project_xy().unit())
    {}
    static orientation reconstruct(const std::vector<point>& orig, const std::vector<point>& xformed);
    static orientation identity() { return { point(0,0,0), point(0,0,0), vector::axis::x() }; }
    
    void set_hmirror(double hmirror) { hmirror_ = hmirror; }
    
    bool defined() const { return gcode_zero_.defined() && cnc_zero_.defined() && rotation_.defined(); }
    
    point operator()(point pt) const
    {
        if (!std::isnan(hmirror_))
            pt.x = 2*hmirror_ - pt.x;
        return (pt - gcode_zero_).rotate(rotation_) + cnc_zero_;
    }
    
    orientation inv() const
    {
        return orientation(cnc_zero_, gcode_zero_, rotation_.mirror_y());
    }
    
    friend std::ostream& operator << (std::ostream& s, const orientation& o)
    {
        return s << "gz=" << o.gcode_zero_ << "; cncz=" << o.cnc_zero_
                 << "; angle=" << std::fixed << vector::axis::x().angle_to(o.rotation_)*180/M_PI;
    }
    
private:
    point gcode_zero_;
    point cnc_zero_;
    vector rotation_;
    double hmirror_ = NAN;
};


