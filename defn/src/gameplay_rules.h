#ifndef GAMEPLAY_RULES_H
#define GAMEPLAY_RULES_H

#include <godot_cpp/core/math.hpp>

namespace defn {

using namespace godot;

struct GameplayRules {
    real_t viewport_width = 1920.0F;
    real_t viewport_height = 1080.0F;
    int world_multiplier = 4;
    real_t belt_top_y = 750.0F;
    real_t belt_bottom_y = 850.0F;
    real_t breach_x = 50.0F;
    real_t spawn_offset = 100.0F;
    real_t friendly_world_margin = 100.0F;
    real_t scroll_trigger_extra_height = 400.0F;
    real_t camera_scroll_step_factor = 0.25F;
};

} // namespace defn

#endif