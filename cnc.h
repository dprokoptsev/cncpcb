#pragma once

#include "geom.h"
#include "utility.h"
#include <iosfwd>
#include <map>
#include <vector>
#include <string>
#include <optional>

class gcmd;

class cnc_machine {
    static constexpr const double MIN_SAFE_HEIGHT = 0.6;

    struct status_t {
        int wcs;
        point position;
        double feed_rate;
        double spindle_speed;
        bool spindle_on;
        bool touches_ground;
        bool idle;
        bool alarm;
    };

public:
    enum class move_mode { safe, unsafe };
    
    explicit cnc_machine(std::iostream& s) { rebind(s); }
    void rebind(std::iostream& s);
    
    void reset();
    
    void wait();
    
    point position() { return status().position; }
    point absolute_position() { return position() + wco(); }
    void redefine_position(point newpos);
    void set_zero() { redefine_position({ 0, 0, 0 }); }
    
    void home(vector axis);
    void home();

    void move(point p, move_mode m = move_mode::safe);
    void move_xy(point p, move_mode m = move_mode::safe);
    void move_z(double z, move_mode m = move_mode::safe);
    double probe();
    
    bool touches_ground() { return status().touches_ground; }
    
    void feed(point p);
    void feed_z(double z);

    double spindle_speed() { return status().spindle_speed; }
    void set_spindle_speed(double speed);
    void set_spindle_on();
    void set_spindle_off();
    bool is_spindle_on() { return status().spindle_on; }
    double feed_rate() { return status().feed_rate; }
    void set_feed_rate(double feed);
    void dwell(double seconds);
    
    void send_gcmd(const gcmd& cmd);
    
    // settings
    vector max_travel() { return vector_setting(130); }
    vector homing_direction() { return mask_setting(23); }
    double homing_pulloff() { return setting(27); }
    
private /*methods*/:
    status_t& status();
    vector wco();
    int wcs() { return status().wcs; }
    void select_wcs(int wcs);
    
    class tmpwcs;
    friend class tmpwcs;
    
    std::string read_hash(const std::string& name, size_t index);
    std::vector<std::string> talk(const std::string& cmd);
    
    double setting(int index);
    vector vector_setting(int index);
    vector mask_setting(int index);
        
private /*fields*/:
    std::iostream* s_;
    std::optional<status_t> status_;
    vector wco_;
    std::map<int, double> settings_;
};

