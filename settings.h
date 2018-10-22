#pragma once

namespace settings {
// TODO: configure those

struct settings {
    double spindle_speed;
    double feed_rate;
    double travel_z;
    double work_z;
};

static const settings DRILL = {
    .spindle_speed = 1000,
    .feed_rate = 75,
    .travel_z = 1,
    .work_z = -1.8
};

static const settings MILL = {
    .spindle_speed = 1000,
    .feed_rate = 75,
    .travel_z = 1,
    .work_z = -0.05
};

struct global_params {
    bool dump_wire = false;
    bool require_z_level_at_tool_change = true;
};

extern global_params g_params;

} // namespace settings
