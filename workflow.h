#pragma once

#include <string>
#include <stdexcept>

class cnc_machine;
class gcode;

class workflow {
public:
    class error: public std::runtime_error {
    public:
        explicit error(const std::string& what): std::runtime_error(what) {}
    };

    explicit workflow(cnc_machine& cnc): cnc_(&cnc) {}

    void load_border(const std::string& filename);
    
    void set_orientation();
    void drill_reference_holes();
    void use_reference_holes();
    
private:
    cnc_machine& cnc() { return *cnc_; }
    
private:
    cnc_machine* cnc_;
    std::unique_ptr<gcode> border_;
    orientation orient_;
};

