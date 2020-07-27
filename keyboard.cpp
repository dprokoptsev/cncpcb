#include "keyboard.h"
#include "utility.h"
#include "cnc.h"
#include "settings.h"
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <map>
#include <string>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <memory>


static const std::map<std::string, key> KEY_SEQS = {
    { "\033[A", key::up },
    { "\033[B", key::down },
    { "\033[D", key::left },
    { "\033[C", key::right },
    { "\033[5~", key::page_up },
    { "\033[6~", key::page_down },
    { "+", key::plus },
    { "-", key::minus },
    { "*", key::multiplies },
    { "/", key::divides },
    { "\r", key::enter },
    { " ", key::space },
    { "q", key::q },
    { "p", key::p },
    { "P", key::shift_p },
    { "Z", key::shift_z },
};


rawtty::rawtty()
{
    struct termios t;
    tcgetattr(0, &saved_);
    tcgetattr(0, &t);
    cfmakeraw(&t);
    tcsetattr(0, TCSAFLUSH, &t);
}

rawtty::~rawtty()
{
    tcsetattr(0, TCSAFLUSH, &saved_);
}

key rawtty::getkey()
{
    std::string seq;
    for (;;) {
        int c = std::cin.get();
        if (c == std::istream::traits_type::eof())
            return key::eof;
        if (c == 0x03)
            throw std::runtime_error("user break");
        
        seq.push_back(c);
        auto i = KEY_SEQS.find(seq);
        if (i != KEY_SEQS.end())
            return i->second;

        while (!seq.empty()
            && std::none_of(KEY_SEQS.begin(), KEY_SEQS.end(), [&seq](const auto& ks) {
                return starts_with(ks.first, seq);
            })
        ){
            seq.erase(0, 1);
        }
    }
}

static volatile sig_atomic_t g_interrupted = 0;

namespace interactive {

static struct winsize g_winsize;
static volatile bool g_have_winsize = false;

static const struct winsize& window_size()
{
    if (!g_have_winsize) {
        if (ioctl(0, TIOCGWINSZ, &g_winsize)) {
            g_winsize.ws_row = 25;
            g_winsize.ws_col = 80;
        }
    }
    return g_winsize;
}

class sighandler_ {
public:
    sighandler_()
    {
        install(SIGINT, &sighandler_::on_int);
        install(SIGWINCH, &sighandler_::on_winch);
    }
    
private:
    static void on_winch(int) { g_have_winsize = false; }
    static void on_int(int) { g_interrupted = 1; }
    
    void install(int sig, void (*handler)(int))
    {
        struct sigaction s;
        memset(&s, 0, sizeof(s));
        s.sa_handler = handler;
        s.sa_flags = 0;
        sigaction(sig, &s, 0);
    }
};

static sighandler_ g_sighandler_;

bool interrupted()
{
    auto v = g_interrupted;
    g_interrupted = 0;
    return !!v;
}

void clear_line()
{
    std::cerr << "\r" << std::string(window_size().ws_col - 1, ' ') << "\r" << std::flush;
}


static const double FEED_RATE = 4;

static double g_feed_xy = 5;
static double g_feed_z = 0.5;
static double g_feed_angle = 5;


void toggle_spindle(cnc_machine& cnc)
{
    if (cnc.is_spindle_on()) {
        cnc.set_spindle_off();
    } else {
        if (cnc.position().z < 1)
            cnc.move_z(1);
        cnc.set_spindle_on();
    }
}


class z_handler {
public:
    explicit z_handler(cnc_machine& cnc, cnc_machine::move_mode mode = cnc_machine::move_mode::safe):
        cnc_(&cnc), mode_(mode)
    {}
        
    bool handle_keypress(key k)
    {
        if (k == key::page_up) {
            cnc_->move_z(cnc_->position().z + g_feed_z, mode_);
        } else if (k == key::page_down) {
            cnc_->move_z(cnc_->position().z - g_feed_z, mode_);
        } else if (k == key::p) {
            cnc_->move_z(cnc_->probe(), cnc_machine::move_mode::unsafe);
        } else if (k == key::multiplies) {
            g_feed_z *= FEED_RATE;
        } else if (k == key::divides) {
            g_feed_z /= FEED_RATE;
        } else {
            return false;
        }

        return true;
    }
    
private:
    cnc_machine* cnc_;
    cnc_machine::move_mode mode_;
};


static bool handle_xy(
    cnc_machine& cnc, orientation orient, key k,
    cnc_machine::move_mode mode = cnc_machine::move_mode::safe
){
    orientation oinv = orient.inv();
    auto move = [&cnc, orient, oinv, mode](vector delta) {
        cnc.move_xy(orient(oinv(cnc.position()) + delta), mode);
    };
    if (k == key::left) {
        move(-vector::axis::x(g_feed_xy));
    } else if (k == key::right) {
        move(vector::axis::x(g_feed_xy));
    } else if (k == key::up) {
        move(vector::axis::y(g_feed_xy));
    } else if (k == key::down) {
        move(-vector::axis::y(g_feed_xy));
    } else if (k == key::plus) {
        g_feed_xy *= FEED_RATE;
    } else if (k == key::minus) {
        g_feed_xy /= FEED_RATE;
    } else if (k == key::shift_z && mode == cnc_machine::move_mode::safe) {
        cnc.move_xy({ 0, 0, 0 });
    } else {
        return false;
    }
    
    return true;
}


point position(cnc_machine& cnc, orientation orient, const std::string& prompt, cnc_machine::move_mode mode)
{
    z_handler zh(cnc, mode);
    orientation oinv = orient.inv();
    
    rawtty raw;
    for (;;) {
        
        auto p = oinv(cnc.position());
        std::cerr << "\r" << prompt
                  << ": x \033[33;1m" << p.x << "\033[0m"
                  << ", y \033[33;1m" << p.y << "\033[0m"
                  << ", z \033[33;1m" << p.z << "\033[0m"
                  << ", feed xy " << g_feed_xy << ", z " << g_feed_z << std::flush;
        
        key k = raw.getkey();
        
        if (handle_xy(cnc, orient, k, mode) || zh.handle_keypress(k)) {
            continue;
        } else if (k == key::space) {
            toggle_spindle(cnc);
        } else if (k == key::enter) {
            clear_line();
            return cnc.position();
        } else if (k == key::q) {
            clear_line();
            return point();
        } 
    }
}

vector angle(cnc_machine& cnc, const std::string& prompt, const point& zero, const vector& orig)
{
    vector v = orig;
    z_handler zh(cnc);
    
    rawtty raw;
    cnc.move_xy(zero + v);
    for (;;) {
        auto p = cnc.position();

        std::cerr << "\r" << prompt
                  << ": angle \033[33;1m" << orig.angle_to(v) * 180 / M_PI << "\xC2\xB0\033[0m"
                  << ", pos " << p
                  << ", feed xy " << g_feed_angle << "\xC2\xB0, z " << g_feed_z << std::flush;
        
        key k = raw.getkey();
        
        if (k == key::left || k == key::up) {
            v = v.rotate(g_feed_angle * M_PI / 180);
            cnc.move_xy(zero + v);
        } else if (k == key::right || k == key::down) {
            v = v.rotate(-g_feed_angle * M_PI / 180);
            cnc.move_xy(zero + v);
        } else if (k == key::plus) {
            g_feed_angle *= 4;
        } else if (k == key::minus) {
            g_feed_angle /= 4;
        } else if (zh.handle_keypress(k)) {
            continue;
        } else if (k == key::enter) {
            clear_line();
            return v;
        } else if (k == key::q) {
            clear_line();
            return vector();
        }
    }
}



std::string point_list::point_desc(size_t idx) const
{
    if (idx < desc_.size())
        return desc_[idx];
    else
        return "pt #" + std::to_string(idx) + " of " + std::to_string(pts_.size());
}

ssize_t point_list::show(const std::string& prompt, size_t start_idx /* = 0*/)
{
    size_t idx = start_idx;
    z_handler zh(cnc());
    
    cnc().move_xy(pts_[idx]);
    
    rawtty raw;
    for (;;) {
        std::cerr << "\r" << prompt << ": \033[33;1m"
                  << point_desc(idx)
                  << "\033[0m (" << pts_[idx].grbl() << "); "
                  << "z=" << cnc().position().z
                  << " (feed " << g_feed_z << ")" << std::flush;
                
        key k = raw.getkey();
        
        if (k == key::left || k == key::up) {
            idx = (idx ? idx : pts_.size()) - 1;
            cnc().move_xy(pts_[idx]);
        } else if (k == key::right || k == key::down) {
            idx = (idx + 1 ) % pts_.size();
            cnc().move_xy(pts_[idx]);
        } else if (zh.handle_keypress(k)) {
            continue;
        } else if (k == key::enter) {
            clear_line();
            return idx;
        } else if (k == key::q || k == key::y) {
            clear_line();
            return -1;
        }
    }
}


bool point_list::edit(const std::string& prompt)
{
    bool changed = false;

    for (size_t idx = 0; idx != pts_.size(); ++idx) {
        point& pt = pts_[idx];
        if (!pt.defined())
            pt = position(cnc(), orientation::identity(), "Position " + point_desc(idx));
    }
    
    ssize_t idx = 0;
    for (;;) {
        idx = show(prompt, idx);
        if (idx == -1)
            return changed;
        
        point newpt = position(cnc(), orientation::identity(), "  Edit " + point_desc(idx));
        if (newpt.defined()) {
            pts_[idx] = newpt;
            changed = true;
        }
    }
}

void point_list::read(const std::string& prompt, size_t count)
{
    std::vector<point> ret;
    for (size_t idx = 0; idx != count; ++idx)
        ret.push_back(position(cnc(), orientation::identity(), prompt + ": " + point_desc(idx)));
    pts_ = std::move(ret);
}

void change_tool(cnc_machine& cnc, const std::string& prompt)
{
    std::string msg = (prompt.empty() ? "Change tool" : prompt);
    z_handler zh(cnc);
    rawtty raw;
    
    for (;;) {
        std::cerr << "\r\033[33;1m" << msg << "\033[0m "
                  << "(feed xy=" << g_feed_xy << " z=" << g_feed_z << ")" << std::flush;
        key k = raw.getkey();
        
        if (k == key::space) {
            toggle_spindle(cnc);
        } else if (!cnc.is_spindle_on() && handle_xy(cnc, orientation::identity(), k)) {
            continue;
        } else if (!cnc.is_spindle_on() && zh.handle_keypress(k)) {
            continue;
        } else if (!cnc.is_spindle_on() && k == key::enter) {
            if (
                cnc.position().distance_to({0, 0, cnc.position().z}) < 2
            ) {
                clear_line();
                break;
            } else {
                clear_line();
                std::cerr << "\rPlease reposition to (0,0) for further probing\r\n";
            }
        }
    }
    
    point p = cnc.position();
    p.z = 0;
    cnc.redefine_position(p);
    cnc.move_z(1);
}

progress_bar::progress_bar(const std::string& prompt, size_t max):
    prompt_(prompt), max_(max), cur_(0), started_at_(time(0)), last_updated_at_(0)
{
    update();
}
    
progress_bar::~progress_bar()
{
    clear_line();
}


void progress_bar::update()
{
    static const int UPDATE_INTERVAL = 2;

    if (settings::g_params.dump_wire)
        return;
    if (last_updated_at_ > time(0) - UPDATE_INTERVAL)
        return;

    std::cerr << "\r" << prompt_ << ": " << std::setfill(' ') << std::setw(3) << (100 * cur_ / max_) << "% [";
    int bar_width = window_size().ws_col - prompt_.size() - 21;
    int filled_width = bar_width * cur_ / max_;
    std::cerr << std::string(filled_width, '#') << std::string(bar_width - filled_width, '.') << "] ";
    
    if (cur_) {
        ssize_t elapsed = time(0) - started_at_;
        ssize_t t = elapsed * (max_ - cur_) / cur_;
        std::cerr << std::setfill(' ') << std::setw(3) << (t / 3600) << ':'
                  << std::setfill('0') << std::setw(2) << ((t % 3600) / 60) << ':'
                  << std::setfill('0') << std::setw(2) << (t % 60);
    }
    std::cerr << std::flush;
    last_updated_at_ = time(0);
}


} // namespace interactive
