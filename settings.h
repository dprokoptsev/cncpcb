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
    .feed_rate = 50,
    .travel_z = 1,
    .work_z = -1.7
};

} // namespace settings
