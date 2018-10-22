#pragma once
#include <vector>
#include <string>
#include <stdexcept>
#include <typeinfo>
#include <cerrno>
#include <cstring>
#include <sstream>

inline bool starts_with(const std::string& s, const std::string& prefix)
{
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

template<class To, class From>
struct lexical_cast_impl_ {
    static To cast(const From& f) {
        std::stringstream s;
        To t;
        if (s << f && s >> t)
            return t;
        else
            throw std::bad_cast();
    }
};

template<class T>
struct lexical_cast_impl_<std::string, T> {
    static std::string cast(const T& t) {
        std::stringstream s;
        s << t;
        return s.str();
    }
};

template<class To, class From>
To lexical_cast(const From& f) { return lexical_cast_impl_<To, From>::cast(f); }

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

template<class T>
std::string join(const T& container, const std::string& separator = "")
{
    std::ostringstream s;
    bool first = true;
    for (const auto& t: container) {
        if (first)
            first = false;
        else
            s << separator;
        s << t;
    }
    return s.str();
}


template<class F, class... Args>
decltype(std::declval<F>()(std::declval<Args>()...))
check_syscall(const std::string& msg, F f, Args... args)
{
    auto ret = f(args...);
    if (ret == -1)
        throw std::runtime_error("cannot " + msg + ": " + strerror(errno));
    return ret;
}
