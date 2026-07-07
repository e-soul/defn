#ifndef GAMEPLAY_RULES_H
#define GAMEPLAY_RULES_H

namespace defn {

struct GameplayRules {
    float viewport_width = 1920.0F;
    float viewport_height = 1080.0F;
    int world_multiplier = 4;
    float belt_top_y = 750.0F;
    float belt_bottom_y = 850.0F;
    float breach_x = 50.0F;
    float spawn_offset = 100.0F;
    float friendly_world_margin = 100.0F;
    float scroll_trigger_extra_height = 400.0F;
    float camera_scroll_step_factor = 0.25F;
};

} // namespace defn

#endif