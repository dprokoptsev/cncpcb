#pragma once

#include "geom.h"
#include "utility.h"
#include <utility>
#include <vector>
#include <string>
#include <functional>
#include <sstream>
#include <memory>
#include <type_traits>


class command {
public:
    class errmsg {
    public:
        bool ok() { return s_.empty(); }
        template<class T> errmsg& operator << (const T& t) { s_ += lexical_cast<std::string>(t); return *this; }
        
        template<class E> void raise_as() const { throw E(s_); }
    private:
        std::string s_;
    };
    
    virtual ~command() {}    
    virtual bool matches(const std::vector<std::string>& argv) const = 0;
    virtual errmsg call(const std::vector<std::string>& args) const = 0;
};


namespace impl {

template<class T, class R, class... Args>
std::tuple<Args...> arg_tuple(R (T::*)(Args...) const);

template<class X>
decltype(arg_tuple(&X::operator())) arg_tuple(X);

template<class T>
constexpr size_t arity(T t) { return std::tuple_size<decltype(arg_tuple(t))>::value; }


template<class T> struct void_t {};

template<class F, class... Args>
std::true_type callable(void_t<decltype(std::declval<F>()(std::declval<Args>()...))>* = 0);

template<class F, class... Args>
std::false_type callable(...);


template<class F, class Iter, class... Args>
command::errmsg call(const F& f, Iter begin, Iter end, const Args&... args)
{
    typedef decltype(arg_tuple(f)) arg_types;
    
    if constexpr (std::tuple_size<arg_types>::value == sizeof...(Args)) {
        if (begin != end)
            return command::errmsg() << "extra arguments";
        f(args...);
        return command::errmsg();
    } else {
        if (begin == end) {
            if constexpr (decltype(callable<F, Args...>(0))::value) {
                f(args...);
                return command::errmsg();
            } else {
                return command::errmsg() << "not enough arguments";
            }
        }
        
        std::stringstream s;
        typename std::decay<typename std::tuple_element<sizeof...(Args), arg_types>::type>::type next_arg;
        if (s << *begin && s >> next_arg)
            return call(f, begin+1, end, args..., next_arg);
        else
            return command::errmsg() << "cannot convert " << *begin << " to required param type";
    }
}

template<class F>
class command_impl: public command {
public:
    command_impl(std::vector<std::string> prefix, F f): prefix_(std::move(prefix)), f_(f) {}

    bool matches(const std::vector<std::string>& argv) const override
    {
        return argv.size() >= prefix_.size() && std::equal(prefix_.begin(), prefix_.end(), argv.begin());
    }
    
    errmsg call(const std::vector<std::string>& argv) const override
    {
        return ::impl::call(f_, argv.begin() + prefix_.size(), argv.end());
    }
    
private:
    std::vector<std::string> prefix_;
    F f_;
};



class command_builder {
public:
    explicit command_builder(const std::string& prefix): prefix_(split<std::string>(prefix, " ")) {}
    
    template<class F>
    std::unique_ptr<command> operator % (F f)
    {
        return std::unique_ptr<command>(new command_impl<F>(std::move(prefix_), f));
    }
    
private:
    std::vector<std::string> prefix_;
};

class command_list {
public:
    command_list();
    command_list& operator << (std::unique_ptr<command> cmd) { cmds_.push_back(std::move(cmd)); return *this; }
    
    static command_list& instance();
    void run();
    std::function<void()>& on_failure() { return on_failure_; }
    
for_testing_only:
    void call(const std::vector<std::string>& args);
    
private:
    std::vector<std::unique_ptr<command>> cmds_;
    std::function<void()> on_failure_;
};

} // namespace impl

inline void run_cmd_loop() { impl::command_list::instance().run(); }

#define COMMAND(name, ...) ::impl::command_list::instance() << ::impl::command_builder(name) % [&](__VA_ARGS__)
#define ON_FAILURE ::impl::command_list::instance().on_failure() = [&]



struct read_rel {
public:
    explicit read_rel(bool& dst): dst_(&dst) {}
    friend std::istream& operator >> (std::istream& in, read_rel rr)
    {
        char c;
        if (!(in >> c))
            return in;
        
        *rr.dst_ = (c == '@');
        if (c != '@')
            in.putback(c);
        return in;
    }
private:
    bool* dst_;
};

struct rel_coord {
public:
    rel_coord(): rel_(true), val_(0) {}
    double resolve(double abs) const { return rel_ ? val_ + abs : val_; }
    friend std::istream& operator >> (std::istream& in, rel_coord& rc)
    {
        return in >> read_rel(rc.rel_) >> rc.val_;
    }
private:
    bool rel_;
    double val_;
};

struct polar_coord {
public:
    polar_coord(): rel_(true), angle_(0), dist_(0) {}
    
    point resolve(point abs) const
    {
        return vector::axis::x(dist_).rotate(angle_ * M_PI / 180) + (rel_ ? abs : point(0,0,0));
    }
    
    friend std::istream& operator >> (std::istream& in, polar_coord& pc)
    {
        char c = 0;
        in >> read_rel(pc.rel_) >> pc.dist_ >> c >> pc.angle_;
        if (c != '>')
            in.setstate(std::istream::badbit);
        return in;
    }
    
private:
    bool rel_;
    double angle_;
    double dist_;
};
