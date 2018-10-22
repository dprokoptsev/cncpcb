#pragma once

#include "geom.h"
#include "utility.h"
#include <iosfwd>


class gcmd;

class cnc_machine {
    static constexpr const double MIN_SAFE_HEIGHT = 0.6;

public:
    enum class move_mode { safe, unsafe };
    
    explicit cnc_machine(std::iostream& s);
    
    void reset();
    
    void wait();
    
    point position();
    void redefine_position(point newpos);
    void set_zero() { redefine_position({ 0, 0, 0 }); }
        
    void move_xy(point p, move_mode m = move_mode::safe);
    void move_z(double z, move_mode m = move_mode::safe);
    double probe();
    double high_precision_probe();
    
    bool touches_ground();
    
    void feed_z(double z);

    double spindle_speed() const { return spindle_speed_; }
    void set_spindle_speed(double speed);
    void set_spindle_on();
    void set_spindle_off();
    bool is_spindle_on();
    void set_feed_rate(double feed) { talk("F" + std::to_string(feed)); }
    void dwell(double seconds);
    
    void send_gcmd(const gcmd& cmd);

    std::vector<std::string> talk(const std::string& cmd);
    
private /*methods*/:
    void get_status();
    vector wco();
        
private /*fields*/:
    std::iostream* s_;
    
    vector wco_;
    point position_;
    
    double feed_rate_;
    double spindle_speed_;
    bool spindle_on_ = false;
    bool touches_ground_ = false;
    bool idle_ = true;
};

