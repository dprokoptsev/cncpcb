#pragma once

#include <iostream>
#include <limits>
#include <cmath>

double deg(double x) { return x*M_PI/180; }


static const double EPSILON = 1e-6;

bool approx_cmp(double a, double b)
{
    return (std::isnan(a) && std::isnan(b))
        || fabs(a - b) < EPSILON;
}

bool approx_cmp(const pt_base& a, const pt_base& b)
    { return approx_cmp(a.x, b.x) && approx_cmp(a.y, b.y) && approx_cmp(a.z, b.z); }

template<class T> class approx_impl_ {
public:
    explicit approx_impl_(T t): t_(t) {}
    friend std::ostream& operator << (std::ostream& s, const approx_impl_<T>& a) { return s << "~" << a.t_; }
    bool operator == (const T& t) const { return approx_cmp(t, t_); }
    bool operator != (const T& t) const { return !(*this == t); }
    friend bool operator == (const T& t, const approx_impl_<T>& a) { return a == t; }
    friend bool operator != (const T& t, const approx_impl_<T>& a) { return !(a == t); }
        
private:
    T t_;
};

template<class T>
approx_impl_<T> approx(const T& t) { return approx_impl_<T>(t); }
