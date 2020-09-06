#pragma once

#include "../geom.h"
#include <iostream>
#include <limits>
#include <cmath>


inline double deg(double x) { return x*M_PI/180; }

namespace approx_impl {

static const double EPSILON = 1e-6;

inline bool approx_cmp(double a, double b)
{
    return (std::isnan(a) && std::isnan(b))
        || fabs(a - b) < EPSILON;
}

inline bool approx_cmp(const pt_base& a, const pt_base& b)
    { return approx_cmp(a.x, b.x) && approx_cmp(a.y, b.y) && approx_cmp(a.z, b.z); }

template<class Range1, class Range2>
inline auto approx_cmp(const Range1& range1, const Range2& range2)
    -> decltype(approx_cmp(*begin(range1), *begin(range2)))
{
    return range1.size() == range2.size()
        && std::equal(
            begin(range1), end(range1), begin(range2),
            [](auto x, auto y){ return approx_cmp(x, y); }
        );
}


template<class T>
inline auto dump_to(std::ostream& out, const T& t) -> decltype(out << t)
{
    return out << t;
}

template<class Range>
inline auto dump_to(std::ostream& out, const Range& range) -> decltype(out << *begin(range))
{
    out << '{';
    bool first = true;
    for (const auto& v: range) {
        if (first)
            first = false;
        else
            out << ", ";
        out << v;
    }
    return out << '}';
}


template<class T> class approx {
public:
    explicit approx(T t): t_(t) {}
    friend std::ostream& operator << (std::ostream& s, const approx<T>& a) { return dump_to(s << "~", a.t_); }
    bool operator == (const T& t) const { return approx_cmp(t, t_); }
    bool operator != (const T& t) const { return !(*this == t); }
    friend bool operator == (const T& t, const approx<T>& a) { return a == t; }
    friend bool operator != (const T& t, const approx<T>& a) { return !(a == t); }
        
private:
    T t_;
};

} // namespace approx_impl

template<class T>
approx_impl::approx<T> approx(const T& t) { return approx_impl::approx<T>(t); }
