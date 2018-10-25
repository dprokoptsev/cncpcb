#include "cnc.h"
#include "serial.h"
#include "geom.h"
#include "keyboard.h"
#include "gcode.h"
#include "workflow.h"
#include "settings.h"
#include <string>
#include <fstream>
#include <algorithm>

#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>


bool readline(const std::string& prompt, std::string& dest)
{
    char* s = ::readline(prompt.c_str());
    if (s) {
        if (*s)
            add_history(s);
        dest = std::string(s);
        free(s);
        return true;
    } else {
        return false;
    }
}


void usage()
{
    std::cerr << "Usage: cnc [-d] <device>" << std::endl;
    ::exit(1);
}


int main(int argc, char** argv)
{
    try {
        int opt;
        
        while ((opt = getopt(argc, argv, "dZ")) != -1) {
            if (opt == 'd') {
                settings::g_params.dump_wire = true;
            } else if (opt == 'Z') {
                settings::g_params.require_z_level_at_tool_change = false;
            } else {
                usage();
            }
        }
        
        if (optind >= argc)
            usage();
        
        serial_port tty(argv[optind]);
        cnc_machine cnc(tty);
        std::unique_ptr<workflow> w(new workflow(cnc));
        
        std::string cmd;
        while (readline("> ", cmd)) {
            auto args = split<std::string>(cmd, " ");
            cmd = args.front();
            args.erase(args.begin());
            
            try {
            
                if (cmd == "reset") {
                    if (args.empty() || args[0] == "cnc")
                        cnc.reset();
                    if (args.empty() || args[0] == "workflow")
                        w.reset(new workflow(cnc));
                } else if (cmd == "pos") {
                    std::cout << cnc.position() << std::endl;
                } else if (cmd == "setzero") {
                    interactive::position(cnc, "Set zero point", cnc_machine::move_mode::unsafe);
                    cnc.set_zero();
                } else if (cmd == "vi") {
                    interactive::position(cnc, "Interactive position");
                } else if (cmd == "hprobe") {
                    std::cout << cnc.high_precision_probe() << std::endl;
                } else if ((cmd == "gcode" || cmd == ".") && !args.empty()) {
                    bool dump = settings::g_params.dump_wire;
                    settings::g_params.dump_wire = true;
                    cnc.talk(join(args, " "));
                    settings::g_params.dump_wire = dump;
                    
                } else if (cmd == "dump_wire" && !args.empty()) {
                    settings::g_params.dump_wire = (args[0] == "on");
                    
                } else if (cmd == "load" && args.size() >= 2) {
                    if (args[0] == "border")
                        w->load_border(args[1]);
                    else if (args[0] == "drill")
                        w->load_drill(args[1]);
                    else if (args[0] == "mill")
                        w->load_mill(args[1]);
                    else
                        std::cerr << "Unknown layer '" << args[0] << "'" << std::endl;

                } else if (cmd == "mirror") {
                    w->mirror();
                    std::cout << "Mirror ON" << std::endl;
                } else if (cmd == "orient") {
                    w->set_orientation(args.empty() ? 0 : lexical_cast<double>(args[0]) * M_PI / 180);
                    std::cerr << "Orientation: " << w->orientation() << std::endl;
                } else if (cmd == "drillrefs") {
                    w->drill_reference_points();
                } else if (cmd == "userefs") {
                    w->use_reference_points();
                    std::cerr << "Orientation: " << w->orientation() << std::endl;
                    
                } else if (cmd == "show" && !args.empty()) {
                    auto xform = [&w](const std::vector<point>& pts) {
                        std::vector<point> xf;
                        std::transform(pts.begin(), pts.end(), std::back_inserter(xf), w->orientation());
                        return xf;
                    };
                    if (args[0] == "refpts")
                        interactive::point_list(cnc, xform(w->reference_points())).show("Reference points");
                    else if (args[0] == "drills")
                        interactive::point_list(cnc, xform(w->drills())).show("Drills");
                    else
                        std::cerr << "Unknown layer " << args[0] << std::endl;

                } else if (cmd == "drill") {
                    w->drill();
                } else if (cmd == "mill") {
                    w->mill();
                } else if (cmd == "hmap+mill") {
                    w->scan_height_map();
                    w->mill();
                } else if (cmd == "cut") {
                    w->cut();
                } else if (cmd == "resume") {
                    w->resume();

                } else if (cmd == "dump" && args.size() == 2) {
                    if (args[0] == "mill")
                        w->dump_mill(args[1]);
                    else
                        std::cerr << "unknown layer '" << args[0] << "'" << std::endl;
                    
                } else if (cmd == "hmap" && !args.empty()) {
                    if (args[0] == "scan")
                        w->scan_height_map();
                    else if (args[0] == "load" && args.size() >= 2)
                        w->load_height_map(args[1]);
                    else if (args[0] == "save" && args.size() >= 2)
                        w->save_height_map(args[1]);
                    else
                        std::cerr << "unknown subcommand 'hmap " << args[0] << "'" << std::endl;
                    
                } else if (cmd == "status") {
                    
                    std::cout << "position: " << cnc.position()
                              << "; spindle: " << (cnc.is_spindle_on() ? "ON" : "off")
                              << "; probe: " << (cnc.touches_ground() ? "YES": "no")
                              << std::endl;

                } else if (cmd == "spindle" && !args.empty()) {
                    
                    if (args[0] == "on")
                        cnc.set_spindle_on();
                    else
                        cnc.set_spindle_off();
                    
                } else if (cmd == "chtool") {
                    
                    interactive::change_tool(cnc, args.empty() ? std::string() : args[0]);
                    
                } else {
                    std::cout << "unknown command" << std::endl;
                }
                
            }
            catch (std::exception& e) {
                cnc.set_spindle_off();
                std::cerr << "\033[31;1m" << e.what() << "\033[0m" << std::endl;
            }
        }
        
        std::cout << std::endl;
        return 0;
    }
    catch (std::runtime_error& e) {
        std::cerr << "cnc: " << e.what() << std::endl;
        return 1;
    }
}
