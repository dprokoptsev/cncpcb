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
            std::vector<std::string> gcode_words = args;
            gcode_words.erase(gcode_words.begin());
            gcmd gc = gcmd::parse(o.inv()(cnc().position()), join(gcode_words, " ")).xform_by(o);
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
        
        while ((opt = getopt(argc, argv, "d")) != -1) {
            if (opt == 'd') {
                settings::g_params.dump_wire = true;
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
        bool move_orient = true;
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
            cnc.redefine_position({ 0, 0, probe_height });
        };
        COMMAND("sethzero") {
            interactive::position(
                cnc, orientation::identity(),
                "Set horizontal zero point", cnc_machine::move_mode::unsafe
            );
            cnc.redefine_position({ 0, 0, cnc.position().z });
        };
        COMMAND("setvzero") {
            interactive::position(
                cnc, orientation::identity(),
                "Set vertical zero point", cnc_machine::move_mode::unsafe
            );
            cnc.redefine_position({ cnc.position().x, cnc.position().y, probe_height });
        };
        
        COMMAND("home x") { cnc.home(vector::axis::x()); };
        COMMAND("home y") { cnc.home(vector::axis::y()); };
        COMMAND("home z") { cnc.home(vector::axis::z()); };
        COMMAND("home") { cnc.home(); };
        
        COMMAND("move", rel_coord x, rel_coord y, rel_coord z = {}) {
            point pos = maybe_orient().inv()(cnc.position());
            cnc.move(maybe_orient()({
                x.resolve(pos.x),
                y.resolve(pos.y),
                z.resolve(pos.z)
            }));
        };
        COMMAND("move", polar_coord xy, rel_coord z = {}) {
            point pos = maybe_orient().inv()(cnc.position());
            point pxy = xy.resolve(maybe_orient().inv()(cnc.position()));
            cnc.move(maybe_orient()({ pxy.x, pxy.y, z.resolve(pos.z) }));
        };
        COMMAND("feed", rel_coord x, rel_coord y, rel_coord z = {}) {
            point pos = maybe_orient().inv()(cnc.position());
            cnc.feed(maybe_orient()({
                x.resolve(pos.x),
                y.resolve(pos.y),
                z.resolve(pos.z)
            }));
        };
        COMMAND("feed", polar_coord xy, rel_coord z = {}) {
            point pos = xy.resolve(maybe_orient().inv()(cnc.position()));
            cnc.feed(maybe_orient()({ pos.x, pos.y, z.resolve(pos.z) }));
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
        COMMAND("orient", double gx, double gy, double cncx, double cncy, double angle) {
            w->set_orientation(orientation(
                point(gx, gy, 0),
                point(cncx, cncy, 0),
                vector::axis::x().rotate(angle * M_PI / 180)
            ));
        };
        COMMAND("orient", double gx, double gy) {
            w->set_orientation(orientation::reconstruct(
                {{0,0,0}, {gx,gy,0}},
                {{0,0,0}, cnc.position().project_xy()}
            ));
        };
        COMMAND("orient -") {
            w->set_orientation(orientation(
                point(0, 0, 0),
                point(0, 0, 0),
                vector::axis::x()
            ));
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
        COMMAND("hmap zero") { w->zero_height_map(); };
        
        COMMAND("spindle on") { cnc.set_spindle_on(); };
        COMMAND("spindle off") { cnc.set_spindle_off(); };
        COMMAND("set spindle_speed", double spd) { cnc.set_spindle_speed(spd); };
        COMMAND("set feed_rate", double feed) { cnc.set_feed_rate(feed); };
        
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
        COMMAND("set move_orient", bool b) { move_orient = b; };
       
        depth_list shape_depths;
        
        double tool_width = 5;
        auto mill_shape = [&](polyline shape) {
            polyline layered = shape_depths(std::move(shape));
            auto pos = cnc.position();

            std::vector<gcmd> gcmds;
            for (const point& pt: layered)
                gcmds.push_back(gcmd("G1", pt));
            gcode gc(gcmds.begin(), gcmds.end());
            gc.break_long_legs();
            gc.xform_by([o = orient(), d = cnc.position().to_vector().project_xy()](point pt){ return o(pt) + d; });
            
            cnc.move_xy(gc.frontpt());
            cnc.set_spindle_on();
            cnc.dwell(2);
            gc.send_to(cnc);
            
            cnc.set_spindle_off();
            if (cnc.position().z < 1)
                cnc.move_z(1);
            cnc.move(pos);
        };
        COMMAND("set shape_depth", double d) { shape_depths = depth_list(d); };
        COMMAND("set shape_depth", double d1, double d2, double step) {
            shape_depths = depth_list(d1, d2, step);
            std::cout << "Cut depths: ";
            for (double d: shape_depths.depths())
                std::cout << std::fixed << std::setprecision(2) << d << "; ";
        };
        
        COMMAND("set tool_width", double w) { tool_width = w; };

        COMMAND("shape segment", double w, double h) { mill_shape(shapes::segment({ w, h, 0 })); };
        COMMAND("shape circle", double r) { mill_shape(shapes::circle(r)); };
        COMMAND("shape fillcircle", double r) { mill_shape(shapes::filled_circle(0, r, tool_width)); };
        COMMAND("shape fillcircle", double r1, double r2) { mill_shape(shapes::filled_circle(r1, r2, tool_width)); };
        COMMAND("shape box", double w, double h) { mill_shape(shapes::box(w, h)); };
        COMMAND("shape box", double w, double h, double r) { mill_shape(shapes::box(w, h, r)); };
        COMMAND("shape fillbox", double w, double h) { mill_shape(shapes::filled_box(w, h, tool_width)); };
                
        impl::command_list::instance() << std::make_unique<gcode_command>(w);

        ON_FAILURE {
            cnc.reset();
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
