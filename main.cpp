#include "cnc.h"
#include "serial.h"
#include "geom.h"
#include "keyboard.h"
#include "gcode.h"
#include "workflow.h"
#include "settings.h"
#include "shapes.h"
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

        std::string ttyname = argv[optind];
        
        auto tty = std::make_unique<serial_port>(ttyname);
        cnc_machine cnc(*tty);
        
        std::map<std::string, std::unique_ptr<workflow>> workflows;
        workflows["default"] = std::make_unique<workflow>(cnc);
        
        std::unique_ptr<workflow>* wh = &workflows["default"];
        workflow* w = wh->get();
        bool move_orient = false;
        double probe_height = 0;
        
        auto orient = [&w]() -> orientation {
            if (!w->orientation().defined())
                throw std::runtime_error("command requires orientation defined");
            return w->orientation();
        };

        std::string cmd;
        while (readline("> ", cmd)) {
            auto args = split<std::string>(cmd, " ");
            cmd = args.front();
            args.erase(args.begin());
            
            try {
            
                if (cmd == "reset" && !args.empty()) {
                    if (args[0] == "cnc") {
                        cnc.reset();
                    } else if (args[0] == "tty") {
                        tty.reset();
                        tty = std::make_unique<serial_port>(ttyname);
                        cnc.rebind(*tty);
                    } else if (args[0] == "workflow") {
                        wh->reset(new workflow(cnc));
                        w = wh->get();
                        std::cerr << "workflow reset" << std::endl;
                    } else {
                        std::cerr << "unknown entity" << std::endl;
                    }
                } else if (cmd == "pos") {
                    std::cout << cnc.position() << std::endl;
                } else if (cmd == "setzero") {
                    interactive::position(
                        cnc, orientation::identity(),
                        "Set zero point", cnc_machine::move_mode::unsafe
                    );
                    cnc.redefine_position({0, 0, probe_height});
                } else if (cmd == "vi") {
                    interactive::position(
                        cnc, move_orient ? orient() : orientation::identity(),
                        "Interactive position"
                    );
                } else if (cmd == "move" || cmd == "feed") {
                    
                    auto to_coord = [&cnc, orient](const std::string& s, double (point::*coord)) {
                        if (s[0] == '@') {
                            return lexical_cast<double>(s.substr(1)) + orient().inv()(cnc.position()).*coord;
                        } else {
                            return lexical_cast<double>(s);
                        }
                    };
                    std::string xy = args[0];
                    point pt;
                    if (xy.find('<') != std::string::npos) {
                        point base(0,0,0);
                        if (xy[0] == '@') {
                            base = orient().inv()(cnc.position());
                            xy.erase(0);
                        }
                        double dist = lexical_cast<double>(xy.substr(xy.find('<')));
                        double angle = lexical_cast<double>(xy.substr(xy.find('>') + 1));
                        pt = vector::axis::x(dist).rotate(angle * M_PI / 180) + base;
                    } else {
                        pt = point(
                            to_coord(args[0], &point::x),
                            to_coord(args[1], &point::y),
                            to_coord(args.size() >= 3 ? args[2] : "@0", &point::z)
                        );
                    }
                    
                    pt = orient()(pt);
                    
                    if (cmd == "move")
                        cnc.move(pt);
                    else
                        cnc.feed(pt);
                    
                    
                } else if (cmd == "probe") {
                    for (size_t i = 0; i != (args.empty() ? 1 : lexical_cast<size_t>(args[0])); ++i) {
                        cnc.move_z(cnc.position().z + 0.5 + double(rand()) / RAND_MAX);
                        std::cout << cnc.probe() << std::endl;
                    }
                } else if (cmd == "hprobe") {
                    for (size_t i = 0; i != (args.empty() ? 1 : lexical_cast<size_t>(args[0])); ++i) {
                        cnc.move_z(cnc.position().z + 0.5 + double(rand()) / RAND_MAX);
                        std::cout << cnc.high_precision_probe() << std::endl;
                    }
                } else if ((cmd == "gcode" || cmd == ".") && !args.empty()) {
                    bool dump = settings::g_params.dump_wire;
                    settings::g_params.dump_wire = true;
                    gcmd gc = gcmd::parse(orient().inv()(cnc.position()), join(args, " ")).xform_by(orient());
                    cnc.send_gcmd(gc);
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
                    auto xform = [&orient](const std::vector<point>& pts) {
                        std::vector<point> xf;
                        std::transform(pts.begin(), pts.end(), std::back_inserter(xf), orient());
                        return xf;
                    };
                    if (args[0] == "orient") {
                        std::cerr << "Orientation" << orient() << std::endl;
                    } else if (args[0] == "refpts") {
                        interactive::point_list(cnc, xform(w->reference_points())).show("Reference points");
                    } else if (args[0] == "drills") {
                        std::vector<point> pts;
                        std::vector<std::string> dias;
                        for (const auto& d: w->drills()) {
                            pts.push_back(d.center);
                            dias.push_back("dia=" + std::to_string(d.radius * 2));
                        }
                        interactive::point_list(cnc, xform(pts), dias).show("Drills");
                    } else {
                        std::cerr << "Unknown layer " << args[0] << std::endl;
                    }

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
                    
                } else if (cmd == "set" && args.size() >= 2) {
                    if (args[0] == "workflow") {
                        wh = &workflows[args[1]];
                        if (!*wh)
                            *wh = std::make_unique<workflow>(cnc);
                        w = wh->get();
                        std::cerr << "switched to workflow " << args[1] << std::endl;
                    } else if (args[0] == "z_adjust") {
                        w->adjust_z(lexical_cast<double>(args[1]));
                    } else if (args[0] == "probe_height") {
                        probe_height = lexical_cast<double>(args[1]);
                    } else if (args[0] == "orient" && args.size() == 6) {
                        w->set_orientation(orientation(
                            point(lexical_cast<double>(args[1]), lexical_cast<double>(args[2]), 0),
                            point(lexical_cast<double>(args[3]), lexical_cast<double>(args[4]), 0),
                            vector::axis::x().rotate(lexical_cast<double>(args[5]) * M_PI / 180)
                        ));
                    } else if (args[0] == "move_orient") {
                        move_orient = lexical_cast<int>(args[1]);
                        std::cerr << "move " << (move_orient ? "" : "NOT ") << "subject to orient xform" << std::endl;
                    } else {
                        std::cerr << "unknown parameter" << std::endl;
                    }
                    
                } else if (cmd == "shape" && args.size() >= 1) {
                    
                    auto send = [&](gcode gc) {
                        gc.xform_by(orient());
                        gc.xform_by([d = cnc.position().to_vector()](point pt) { return pt + d; });
                        gc.send_to(cnc);
                    };
                    
                    if (args[0] == "circle" && args.size() == 2)
                        send(shapes::circle(lexical_cast<double>(args[1])));
                    else if (args[0] == "fillcircle" && args.size() == 3)
                        send(shapes::filled_circle(lexical_cast<double>(args[1]), lexical_cast<double>(args[2])));
                    else
                        std::cerr << "unknown shape " << args[1] << std::endl;
                    
                    cnc.set_spindle_off();
                    cnc.move_z(1);
                    
                } else {
                    std::cerr << "unknown command" << std::endl;
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
