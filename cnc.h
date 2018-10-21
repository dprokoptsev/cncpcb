#pragma once

#include "geom.h"
#include "utility.h"
#include <iosfwd>


class gcmd;

class cnc_machine {
    static constexpr const double MIN_SAFE_HEIGHT = 1;

public:
    explicit cnc_machine(std::iostream& s);
    
    void reset();
    
    point position();
    void redefine_position(point newpos);
    void set_zero() { redefine_position({ 0, 0, 0 }); }
        
    void move_xy(point p);
    void move_z(double z);    
    double probe();
    
    void feed_z(double z);

    double spindle_speed() const { return spindle_speed_; }
    void set_spindle_speed(double speed);
    void set_spindle_on();
    void set_spindle_off();
    bool is_spindle_on();
    
    void set_feed_rate(double feed) { talk("F" + std::to_string(feed)); }
    
    void dwell(double seconds);
    
    void send_gcmd(const gcmd& cmd);
    
private /*methods*/:
    
public:
    std::vector<std::string> talk(const std::string& cmd);
    void get_status();
    vector wco();
        
private /*fields*/:
    std::iostream* s_;
    
    vector wco_;
    point position_;
    
    double feed_rate_;
    double spindle_speed_;
    bool spindle_on_ = false;
};

