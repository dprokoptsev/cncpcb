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
        return s << std::setprecision(3) << p.x << ','
                 << std::setprecision(3) << p.y << ','
                 << std::setprecision(3) << p.z;
    }
    
    std::string grbl() const
    {
        std::ostringstream s;
        s << "X" << std::setprecision(3) << x
          << " Y" << std::setprecision(3) << y
          << " Z" << std::setprecision(3) << z;
        return s.str();
    }
};


class vector: public pt_base {
public:
    using pt_base::pt_base;
    
    struct axis {
        static vector x(double x) { return { x, 0, 0 }; }
        static vector y(double y) { return { 0, y, 0 }; }
        static vector z(double z) { return { 0, 0, z }; }
    };
    
    vector operator + (const vector& v) const { return { x + v.x, y + v.y, z + v.z }; }
    vector operator - (const vector& v) const { return { x - v.x, y - v.y, z - v.z }; }
    vector operator * (double k) const { return { x*k, y*k, z*k }; }
    vector operator / (double k) const { return { x/k, y/k, z/k }; }
    friend vector operator * (double k, const vector& v) { return v*k; }

    vector& operator += (const vector& v) { return (*this = *this + v); }
    vector& operator -= (const vector& v) { return (*this = *this - v); }
    vector& operator *= (double k) { return (*this = *this * k); }
    vector& operator /= (double k) { return (*this = *this / k); }
    
    vector rotate(double angle) const { return { x*cos(angle) - y*sin(angle), y*cos(angle)+x*sin(angle), z }; }
};


class point: public pt_base {
public:
    using pt_base::pt_base;
    
    point operator + (const vector& v) const { return { x + v.x, y + v.y, z + v.z }; }
    point operator - (const vector& v) const { return { x - v.x, y - v.y, z - v.z }; }
    
    friend point operator + (const vector& v, const point& pt) { return { v.x + pt.x, v.y + pt.y, v.z + pt.z }; }
    vector operator - (const point& p) const { return { x - p.x, y - p.y, z - p.z }; }
    
    point rotate(const point& center, double angle) const { return center + (*this - center).rotate(angle); }
};
