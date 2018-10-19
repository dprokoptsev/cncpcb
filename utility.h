#pragma once
#include <vector>
#include <string>
#include <stdexcept>
#include <typeinfo>


template<class To, class From>
To lexical_cast(const From& f)
{
    std::stringstream s;
    To t;
    if (s << f && s >> t)
        return t;
    else
        throw std::bad_cast();
}


template<class T>
std::vector<T> split(const std::string& s, const std::string& sep)
{
    std::vector<T> ret;
    size_t pos = 0;
    for (;;) {
        size_t p = s.find(sep, pos);
        ret.push_back(lexical_cast<T>(s.substr(pos, p == std::string::npos ? p : p - pos)));
        if (p == std::string::npos)
            return ret;
        else
            pos = p + 1;
    }
    return ret;
}
