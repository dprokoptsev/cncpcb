#include "cnc.h"
#include "serial.h"
#include "geom.h"
#include "keyboard.h"
#include "gcode.h"
#include "workflow.h"
#include <string>
#include <fstream>
#include <algorithm>

#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

bool g_dump_wire = false;

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
        
        while ((opt = getopt(argc, argv, "d")) != -1) {
            if (opt == 'd') {
                g_dump_wire = true;
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
                } else if (!cmd.empty() && cmd[0] == '.') {
                    cnc.talk(cmd.substr(1));
                    
                } else if (cmd == "border" && !args.empty()) {
                    w->load_border(args[0]);
                } else if (cmd == "orient") {
                    w->set_orientation();
                } else if (cmd == "drillrefs") {
                    w->drill_reference_holes();
                } else if (cmd == "userefs") {
                    w->use_reference_holes();
                    
                } else if (cmd == "status") {
                    
                    std::cout << "position: " << cnc.position()
                              << "; spindle: " << (cnc.is_spindle_on() ? "ON" : "off") << std::endl;
                } else if (cmd == "spindle" && !args.empty()) {
                    
                    if (args[0] == "on")
                        cnc.set_spindle_on();
                    else
                        cnc.set_spindle_off();
                    
                } else if (cmd == "chtool") {
                    
                    interactive::change_tool(cnc, args.empty() ? "change tool" : args[0]);
                    
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
