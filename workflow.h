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
    void load_drill(const std::string& filename);
    
    const ::orientation& orientation() const { return orient_; }
    void set_orientation();
    void drill_reference_holes();
    void use_reference_holes();
    
    void drill();
    
private:
    cnc_machine& cnc() { return *cnc_; }
    void require_border();
    void require_orientation();
    
private:
    cnc_machine* cnc_;
    std::unique_ptr<gcode> border_;
    ::orientation orient_;
    
    std::unique_ptr<gcode> drill_;
};

