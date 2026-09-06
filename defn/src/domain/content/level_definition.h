// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LEVEL_DEFINITION_H
#define LEVEL_DEFINITION_H

#include "content_values.h"

#include <string>
#include <vector>

namespace defn {

struct SpawnDefinition {
    double time = 0.0;
    std::string type = "jackal";
};

struct WaveDefinition {
    int wave_number = 1;
    std::vector<SpawnDefinition> spawns;
    // What every hostile in this wave hits for, as a multiple of its catalog damage. Authored levels leave it at 1
    // and endless raises it per wave. It rides on the wave rather than being set on the scheduler between waves
    // because a wave's spawns and its wave-changed signal arrive in the same tick -- a setter would apply the new
    // scale to some of the wave's own spawns and the old one to the rest.
    double damage_scale = 1.0;
};

struct LevelDefinition {
    int level_id = 0;
    std::string name;
    int starting_core_resource = 100;
    int base_integrity = 3;
    Vector2 base_position_ratio{.x = 0.0760416667F, .y = 0.7407407407F};
    Vector2 belt_width_ratio{.x = 0.6944444444F, .y = 0.7870370370F};
    std::string background_path;
    std::vector<WaveDefinition> waves;
};

} // namespace defn

#endif
