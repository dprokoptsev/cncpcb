#include "cnc.h"
#include "keyboard.h"
#include "workflow.h"
#include "gcode.h"
#include "settings.h"
#include "height_map.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cassert>

static std::vector<point> make_reference_points(const gcode& border)
{
    static const double MARGIN = 0;
    static const double SHIFT = 10;
    auto bbox = border.bounding_box();
    
    std::vector<point> ret;
    ret.push_back(bbox.bottom_left() + vector(-MARGIN, SHIFT, 0));
    ret.push_back(bbox.top_left() + vector(SHIFT, MARGIN, 0));
    ret.push_back(bbox.top_right() + vector(-SHIFT, MARGIN, 0));
    ret.push_back(bbox.bottom_right() + vector(MARGIN, SHIFT, 0));
    
    return ret;
}

static std::vector<std::string> reference_points_desc()
{
    return {
        "lower-left",
        "upper-left",
        "upper-right",
        "lower-right"
    };
}

void workflow::mirror(bool b)
{
    if (border_ || orient_.defined() || drill_ || mill_)
        throw error("reset workflow first");
    mirror_ = b;
}


void workflow::load_border(const std::string& filename)
{
    std::ifstream f(filename);
    border_.reset(new gcode(f));
    std::cerr << "Loaded " << filename << "; board size = " << border_->bounding_box().size() << std::endl;
}

void workflow::require_border() const
{
    if (!border_)
        throw error("border not loaded");
}

void workflow::require_orientation() const
{
    if (!orient_.defined())
        throw error("orientation not defined; run 'orient' or 'userefs'");
}

std::unique_ptr<gcode> workflow::load_gcode(const std::string& filename)
{
    require_border();
    std::ifstream f(filename);
    auto g = std::make_unique<gcode>(f);

    if (!border_->bounding_box().contains(g->bounding_box()))
        throw error("layer exceeds PCB border");

    std::cerr << "Loaded " << filename << "; " << std::distance(g->begin(), g->end()) << " commands" << std::endl;
    return g;
}
    
void workflow::set_orientation(double angle_hint /* = 0 */)
{
    require_border();
    ::orientation orient;    

    bounding_box bbox = border_->bounding_box();
    std::cout << "Size: " << bbox.size() << std::endl;
    vector v = bbox.size();

    v = v.rotate(angle_hint);
    for (;;) {
        point ll = interactive::position(cnc(), orientation::identity(), "Bottom-left corner");
        ll.z = 0;
        v = interactive::angle(cnc(), "Top-right corner", ll, v);

        orient = ::orientation(bbox.bottom_left(), ll, vector::axis::x().rotate(bbox.size().angle_to(v)));
        std::cout << "Orientation: " << orient << std::endl;

        std::vector<point> pts = make_reference_points(*border_);
        std::vector<std::string> desc = reference_points_desc();
        assert(!pts.empty());
        
        pts.push_back(bbox.bottom_left());
        pts.push_back(bbox.top_left());
        pts.push_back(bbox.top_right());
        pts.push_back(bbox.bottom_right());
        for (size_t i = 0; i != 4; ++i)
            desc.push_back("Corner");
        
        std::transform(pts.begin(), pts.end(), pts.begin(), orient);
        
        size_t nearest = std::distance(
            pts.begin(), std::min_element(
                pts.begin(), pts.end(),
                [this](point a, point b) {
                    return cnc().position().distance_to(a) < cnc().position().distance_to(b);
                }
            )
        );

        if (
            interactive::point_list(cnc(), pts, desc)
            .show("Reference points", nearest) != -1
        )
            break;

        cnc().move_xy(ll);
    }
    
    if (mirror_)
        orient.set_hmirror(bbox.center().x);
    orient_ = std::move(orient);
}

void workflow::set_orientation(::orientation o)
{
    if (mirror_ && !border_)
        throw error("load border before setting up orientation");
    orient_ = o;
    if (mirror_)
        orient_.set_hmirror(border_->bounding_box().center().x);
}


void workflow::drill_reference_points()
{
    require_border();
    require_orientation();
    interactive::change_tool(cnc(), "Change tool to drill ~0.5mm");
    
    std::vector<point> refpts = make_reference_points(*border_);
    std::transform(refpts.begin(), refpts.end(), refpts.begin(), orient_);
    
    const auto& settings = ::settings::DRILL;
    cnc().move_z(settings.travel_z);
    cnc().set_spindle_speed(settings.spindle_speed);
    cnc().set_feed_rate(settings.feed_rate);
    cnc().set_spindle_on();
    cnc().dwell(1);
    
    for (const point& pt: refpts) {
        cnc().move_xy(pt);
        cnc().feed_z(settings.work_z);
        cnc().feed_z(0);
        cnc().move_z(settings.travel_z);
    }
    
    cnc().set_spindle_off();
}


void workflow::use_reference_points()
{
    if (!border_)
        throw error("border not loaded");
    
    std::vector<point> origpts = make_reference_points(*border_);
    
    interactive::point_list refpts(cnc(), reference_points_desc());
    refpts.read("Position reference points", origpts.size());
    
    for (;;) {
        for (point& pt: refpts.points())
            pt.z = 0;

        ::orientation o = orientation::reconstruct(origpts, refpts.points());
        
        refpts.points().clear();
        for (const auto& pt: origpts)
            refpts.points().push_back(o(pt));
        
        if (!refpts.edit("Reposition reference points")) {
            if (mirror_)
                o.set_hmirror(border_->bounding_box().center().x);
            orient_ = o;
            break;
        }
    }
}

void workflow::run(const std::string& prompt, const gcode& gc)
{
    require_border();
    require_orientation();
    auto c = std::make_unique<gcode>(gc);
    c->break_long_legs();
    if (height_map_)
        c->xform_by(*height_map_);
    c->xform_by(orient_);
    c->xform_by([this](const point& pt) { return pt + vector::axis::z(z_adjustment_); });
    
    current_ = std::move(c);
    current_->send_to(cnc(), prompt);
    current_.reset();
}


void workflow::drill()
{
    if (!drill_)
        throw error("load drill gcode first");
    run("Drilling", *drill_);
}

void workflow::mill()
{
    if (!mill_)
        throw error("load mill gcode first");
    if (!height_map_)
        throw error("height map not loaded; use 'hmap scan' or 'hmap load'");
    
    run("Milling", *mill_);
}

void workflow::cut()
{
    require_border();
    require_orientation();
    
    interactive::change_tool(cnc(), "Change tool to cutting bit");
    run("Cutting", *border_);
}

void workflow::resume()
{
    if (current_)
        current_->send_to(cnc(), "Working");
    current_.reset();
}

void workflow::dump_layer(const gcode* gc, const std::string& filename) const
{
    require_border();
    require_orientation();
    if (!gc)
        throw error("layer not loaded");
    
    gcode tmp(*gc);
    if (height_map_)
        tmp.xform_by(*height_map_);
    tmp.xform_by(orient_);
    
    std::ofstream f(filename);
    for (const gcmd& cmd: tmp)
        f << cmd << "\n";
    
    std::cerr << "Layer saved to " << filename << std::endl;
}

std::vector<point> workflow::reference_points() const
{
    require_border();
    return make_reference_points(*border_);
}

std::vector<circular_area> workflow::drills() const
{
    require_border();
    if (!drill_)
        throw error("drill not loaded");
    
    static const std::string PROMPT = "Change to tool dia=";
    double dia = 0.6;
    
    std::vector<circular_area> ret;
    for (const auto& cmd: *drill_) {
        if (cmd.equals('G', 1) && cmd.point().z < 0) {
            ret.push_back({{ cmd.point().x, cmd.point().y, 0 }, dia / 2});
        } else if (cmd.letter() == '*' && starts_with(cmd.str_arg(), PROMPT)) {
            dia = lexical_cast<double>(cmd.str_arg().substr(PROMPT.size()));
        }
    }
    
    for (const auto& pt: make_reference_points(*border_))
        ret.push_back({ pt, 0.25 });

    return ret;
}

void workflow::zero_height_map()
{
    auto h = std::make_unique<height_map>(border_->bounding_box(), std::vector<circular_area>());
    for (point& pt: *h)
        pt.z = 0;
    height_map_ = std::move(h);
}

void workflow::scan_height_map()
{
    require_border();
    require_orientation();
    
    auto h = std::make_unique<height_map>(border_->bounding_box(), drills());
    
    interactive::change_tool(cnc(), "Change tool to engraving bit");
    
    interactive::progress_bar progress("Scanning height map", std::distance(h->begin(), h->end()));
    
    double travel_z = settings::MILL.travel_z;
    cnc().move_z(travel_z);
    for (point& pt: *h) {
        cnc().move_xy(orient_(pt));
        pt.z = cnc().probe();
        cnc().move_z(travel_z);
        progress.increment();
    }
    
    height_map_ = std::move(h);
    std::cerr << std::endl;
}

void workflow::load_height_map(const std::string& filename)
{
    require_border();
    
    std::ifstream f(filename);
    auto h = std::make_unique<height_map>(f);

    auto d = h->bounding_box().size() - border_->bounding_box().size();
    if (d.length() > 1e-3)
        throw std::runtime_error("height map size mismatch");

    height_map_ = std::move(h);
}

void workflow::save_height_map(const std::string& filename) const
{
    if (!height_map_)
        throw std::runtime_error("height map not initialized");

    std::ofstream f(filename);
    height_map_->save(f);
}
