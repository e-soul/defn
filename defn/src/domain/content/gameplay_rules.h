// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef GAMEPLAY_RULES_H
#define GAMEPLAY_RULES_H

namespace defn {

// The belt has no right-hand end: the camera advances as far as the front line pushes it, the background tiles
// under it, and nothing clamps a friendly's advance. Only the left edge is a real boundary, because the base is
// standing on it.
struct GameplayRules {
    float viewport_width = 1920.0F;
    float viewport_height = 1080.0F;
    float belt_top_y = 750.0F;
    float belt_bottom_y = 850.0F;
    float breach_x = 50.0F;
    float spawn_offset = 100.0F;
    float scroll_trigger_extra_height = 400.0F;
    float camera_scroll_step_factor = 0.25F;
};

} // namespace defn

#endif