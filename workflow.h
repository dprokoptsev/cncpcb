#pragma once

#include "utility.h"
#include "gcode.h"
#include "height_map.h"
#include <string>
#include <stdexcept>

class cnc_machine;

class workflow {
public:
    class error: public std::runtime_error {
    public:
        explicit error(const std::string& what): std::runtime_error(what) {}
    };

    explicit workflow(cnc_machine& cnc): cnc_(&cnc) {}
    cnc_machine& cnc() { return *cnc_; }
    
    void mirror(bool b);
    
    void load_border(const std::string& filename);
    
    void load_drill(const std::string& filename) { drill_ = load_gcode(filename); }
    void load_mill(const std::string& filename) { mill_ = load_gcode(filename); }
    
    const ::orientation& orientation() const { return orient_; }
    void set_orientation(double angle_hint = 0);
    void set_orientation(::orientation o);
    void drill_reference_points();
    void use_reference_points();
    
    void scan_height_map();
    void save_height_map(const std::string& filename) const;
    void load_height_map(const std::string& filename);
    
    void adjust_z(double adj) { z_adjustment_ = adj; }
    
    void drill();
    void mill();
    void cut();
    
    void resume();
    
    void dump_mill(const std::string& out) const { dump_layer(mill_.get(), out); }

    std::vector<point> reference_points() const;
    std::vector<circular_area> drills() const;

for_testing_only:
    workflow(): cnc_(0) {}
    void set_border(std::unique_ptr<gcode> gcode) { border_ = std::move(gcode); }
    void set_drill(std::unique_ptr<gcode> drill) { drill_ = std::move(drill); }

private:
    void require_border() const;
    void require_orientation() const;
    
    std::unique_ptr<gcode> load_gcode(const std::string& filename);
    void dump_layer(const gcode* gc, const std::string& out) const;
    
    void run(const std::string& prompt, const gcode& gc);
        
private:
    cnc_machine* cnc_;
    std::unique_ptr<gcode> border_;
    ::orientation orient_;
    std::unique_ptr<height_map> height_map_;
    double z_adjustment_ = 0;
    
    std::unique_ptr<gcode> drill_;
    std::unique_ptr<gcode> mill_;
    
    std::unique_ptr<gcode> current_;
    
    bool mirror_ = false;
};

