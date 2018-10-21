#include "keyboard.h"
#include "utility.h"
#include "cnc.h"
#include <termios.h>
#include <unistd.h>
#include <map>
#include <string>
#include <algorithm>
#include <iostream>


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
    { "p", key::p }
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


namespace interactive {

static const double FEED_RATE = 4;

static double g_feed_xy = 20;
static double g_feed_z = 2;
static double g_feed_angle = 10;


class z_handler {
public:
    explicit z_handler(cnc_machine& cnc, cnc_machine::move_mode mode = cnc_machine::move_mode::safe):
        cnc_(&cnc), mode_(mode)
    {}
    
    bool probed() const { return probed_; }
    
    bool handle_keypress(key k)
    {
        if (k == key::page_up) {
            cnc_->move_z(cnc_->position().z + g_feed_z, mode_);
            probed_ = false;
        } else if (k == key::page_down) {
            cnc_->move_z(cnc_->position().z - g_feed_z, mode_);
            probed_ = false;
        } else if (k == key::p) {
            cnc_->probe();
            probed_ = true;
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
    bool probed_ = false;
};

point position(cnc_machine& cnc, const std::string& prompt, cnc_machine::move_mode mode)
{
    z_handler zh(cnc, mode);
    
    rawtty raw;
    for (;;) {
        
        auto p = cnc.position();
        std::cerr << "\r" << prompt
                  << ": x \033[33;1m" << p.x << "\033[0m"
                  << ", y \033[33;1m" << p.y << "\033[0m"
                  << ", z \033[33;1m" << p.z << "\033[0m"
                  << ", feed xy " << g_feed_xy << ", z " << g_feed_z << std::flush;
        
        key k = raw.getkey();
        
        if (k == key::left) {
            cnc.move_xy(cnc.position() - vector::axis::x(g_feed_xy), mode);
        } else if (k == key::right) {
            cnc.move_xy(cnc.position() + vector::axis::x(g_feed_xy), mode);
        } else if (k == key::up) {
            cnc.move_xy(cnc.position() + vector::axis::y(g_feed_xy), mode);
        } else if (k == key::down) {
            cnc.move_xy(cnc.position() - vector::axis::y(g_feed_xy), mode);
        } else if (k == key::plus) {
            g_feed_xy *= FEED_RATE;
        } else if (k == key::minus) {
            g_feed_xy /= FEED_RATE;
        } else if (k == key::enter) {
            std::cerr << "\r\n";
            return cnc.position();
        } else if (k == key::q) {
            std::cerr << "\r\n";
            return point();
        } else if (zh.handle_keypress(k)) {
            continue;
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
            std::cerr << "\r\n";
            return v;
        } else if (k == key::q) {
            std::cerr << "\r\n";
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
            std::cerr << "\r\n";
            return idx;
        } else if (k == key::q || k == key::y) {
            std::cerr << "\r\n";
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
            pt = position(cnc(), "Position " + point_desc(idx));
    }
    
    ssize_t idx = 0;
    for (;;) {
        idx = show(prompt, idx);
        if (idx == -1)
            return changed;
        
        point newpt = position(cnc(), "  Edit " + point_desc(idx));
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
        ret.push_back(position(cnc(), prompt + ": " + point_desc(idx)));
    pts_ = std::move(ret);
}


void change_tool(cnc_machine& cnc, const std::string& msg)
{
    std::cerr << "\n\033[33;1m" << msg << "\033[0m" << std::endl;
    z_handler zh(cnc);
    rawtty raw;
    
    for (;;) {
        key k = raw.getkey();
        
        if (k == key::space) {
            if (cnc.is_spindle_on()) {
                cnc.set_spindle_off();
            } else {
                if (cnc.position().z < 1)
                    cnc.move_z(1);
                cnc.set_spindle_on();
            }
            
        } else if (!cnc.is_spindle_on() && zh.handle_keypress(k)) {
            continue;
        } else if (!cnc.is_spindle_on() && k == key::enter) {
            if (true || zh.probed()) {
                std::cerr << "\r\n";
                break;
            } else {
                std::cerr << "Please probe z\r\n";
            }
        }
    }
    
    point p = cnc.position();
    cnc.redefine_position({ p.x, p.y, 0 });
    cnc.move_z(1);
}


} // namespace interactive
