#include "cnc.h"
#include "serial.h"
#include "geom.h"
#include "keyboard.h"
#include "gcode.h"
#include "workflow.h"
#include "settings.h"
#include "shapes.h"
#include "dispatch.h"
#include <string>
#include <fstream>
#include <algorithm>

#include <unistd.h>


void usage()
{
    std::cerr << "Usage: cnc [-d] <device>" << std::endl;
    ::exit(1);
}


class gcode_command: public command {
public:
    explicit gcode_command(workflow*& w): w_(&w) {}
    
    bool matches(const std::vector<std::string>& args) const override
    {
        return !args.empty() && (args[0] == "gcode" || args[0] == ".");
    }
    
    command::errmsg call(const std::vector<std::string>& args) const override
    {
        std::string line = join(std::vector<std::string>(args.begin() + 1, args.end()), " ");
        bool dump = settings::g_params.dump_wire;
        try {
            if (!(*w_)->orientation().defined())
                throw std::runtime_error("raw gcode requires orientation");
            
            orientation o = (*w_)->orientation();
            settings::g_params.dump_wire = true;
            gcmd gc = gcmd::parse(o.inv()(cnc().position()), join(args, " ")).xform_by(o);
            cnc().send_gcmd(gc);
            settings::g_params.dump_wire = dump;
        }
        catch (...) {
            settings::g_params.dump_wire = dump;
            throw;
        }
        return {};
    }
    
private:
    workflow** w_;
    
    cnc_machine& cnc() const { return (*w_)->cnc(); }
};

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
        auto maybe_orient = [&orient, &move_orient]() -> orientation {
            return move_orient ? orient() : orientation::identity();
        };
        
        COMMAND("reset cnc") { cnc.reset(); };
        COMMAND("reset tty") {
            tty.reset();
            tty = std::make_unique<serial_port>(ttyname);
            cnc.rebind(*tty);
        };
        COMMAND("reset workflow") {
            wh->reset(new workflow(cnc));
            w = wh->get();
            std::cerr << "workflow reset" << std::endl;
        };
        
        COMMAND("pos") { std::cout << cnc.position() << std::endl; };
        COMMAND("status") {
            std::cout << "position: " << cnc.position()
                      << "; spindle: " << (cnc.is_spindle_on() ? "ON" : "off")
                      << "; probe: " << (cnc.touches_ground() ? "YES": "no")
                      << std::endl;
        };
        
        COMMAND("vi") { interactive::position(cnc, maybe_orient(), "Interactive position"); };
        
        COMMAND("setzero") {
            interactive::position(
                cnc, orientation::identity(),
                "Set zero point", cnc_machine::move_mode::unsafe
            );
        };
        
        COMMAND("move", rel_coord x, rel_coord y, rel_coord z = {}) {
            point pos = cnc.position();
            cnc.move({
                x.resolve(pos.x),
                y.resolve(pos.y),
                z.resolve(pos.z)
            });
        };
        COMMAND("move", polar_coord xy, rel_coord z = {}) {
            point pos = xy.resolve(cnc.position());
            cnc.move({ pos.x, pos.y, z.resolve(pos.z) });
        };
        COMMAND("feed", rel_coord x, rel_coord y, rel_coord z = {}) {
            point pos = cnc.position();
            cnc.feed({
                x.resolve(pos.x),
                y.resolve(pos.y),
                z.resolve(pos.z)
            });
        };
        COMMAND("feed", polar_coord xy, rel_coord z = {}) {
            point pos = xy.resolve(cnc.position());
            cnc.feed({ pos.x, pos.y, z.resolve(pos.z) });
        };
        
        COMMAND("dump wire", bool b) { settings::g_params.dump_wire = b; };
        
        COMMAND("load border", const std::string& file) { w->load_border(file); };
        COMMAND("load mill", const std::string& file) { w->load_mill(file); };
        COMMAND("load drill", const std::string& file) { w->load_drill(file); };
        
        COMMAND("mirror", bool b = true) { w->mirror(b); };

        COMMAND("orient", double angle_hint = 0) {
            w->set_orientation(angle_hint * M_PI / 180);
            std::cerr << "Orientation: " << w->orientation() << std::endl;
        };
        
        COMMAND("drillrefs") { w->drill_reference_points(); };
        COMMAND("userefs") {
            w->use_reference_points();
            std::cerr << "Orientation: " << w->orientation() << std::endl;
        };
        
        
        auto xform = [&orient](const std::vector<point>& pts) {
            std::vector<point> xf;
            std::transform(pts.begin(), pts.end(), std::back_inserter(xf), orient());
            return xf;
        };

        COMMAND("show orient") { std::cout << orient() << std::endl; };
        COMMAND("show refpts") {
            interactive::point_list(cnc, xform(w->reference_points())).show("Reference points");
        };
        COMMAND("show drills") {
            std::vector<point> pts;
            std::vector<std::string> dias;
            for (const auto& d: w->drills()) {
                pts.push_back(d.center);
                dias.push_back("dia=" + std::to_string(d.radius * 2));
            }
            interactive::point_list(cnc, xform(pts), dias).show("Drills");
        };
        
        
        COMMAND("run drill") { w->drill(); };
        COMMAND("run mill") { w->mill(); };
        COMMAND("run hmap+mill") {
            w->scan_height_map();
            w->mill();
        };
        COMMAND("run cut") { w->cut(); };
        COMMAND("resume") { w->resume(); };
        
        COMMAND("dump mill", const std::string& filename) { w->dump_mill(filename); };
        
        COMMAND("hmap scan") { w->scan_height_map(); };
        COMMAND("hmap load", const std::string& filename) { w->load_height_map(filename); };
        COMMAND("hmap save", const std::string& filename) { w->save_height_map(filename); };
        
        COMMAND("spindle on") { cnc.set_spindle_on(); };
        COMMAND("spindle off") { cnc.set_spindle_off(); };
        
        COMMAND("chtool", const std::string& prompt) { interactive::change_tool(cnc, prompt); };
        
        COMMAND("set workflow", const std::string& wfname) {
            wh = &workflows[wfname];
            if (!*wh)
                *wh = std::make_unique<workflow>(cnc);
            w = wh->get();
            std::cerr << "switched to workflow " << wfname << std::endl;
        };
        COMMAND("set z_adjust", double z) { w->adjust_z(z); };
        COMMAND("set probe_height", double h) { probe_height = h; };
        COMMAND("set orient", double gx0, double gy0, double cx0, double cy0, double rot) {
            w->set_orientation({
                point { gx0, gy0, 0 },
                point { cx0, cy0, 0 },
                vector::axis::x().rotate(rot * M_PI / 180)
            });
        };
        COMMAND("set move_orient", bool b) { move_orient = b; };
        
        
        auto mill_shape = [&](gcode gc) {
            gc.xform_by(orient());
            gc.xform_by([d = cnc.position().to_vector()](point pt) { return pt + d; });
            gc.send_to(cnc);
            
            cnc.set_spindle_off();
            if (cnc.position().z < 1)
                cnc.move_z(1);
        };
        COMMAND("shape circle", double r) { mill_shape(shapes::circle(r)); };
        COMMAND("shape fullcircle", double r, double w) { mill_shape(shapes::filled_circle(r, w)); };
        
        impl::command_list::instance() << std::make_unique<gcode_command>(w);

        ON_FAILURE {
            cnc.set_spindle_off();
        };
        run_cmd_loop();
        cnc.set_spindle_off();

        return 0;
    }
    catch (std::runtime_error& e) {
        std::cerr << "cnc: " << e.what() << std::endl;
        return 1;
    }
}
