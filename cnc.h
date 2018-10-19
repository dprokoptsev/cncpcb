#pragma once

#include "geom.h"
#include <iosfwd>


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
    
private /*methods*/:
    
public:
    std::vector<std::string> talk(const std::string& cmd);    
    vector wco();
        
private /*fields*/:
    std::iostream* s_;
    
    vector wco_;
    point position_;
};

