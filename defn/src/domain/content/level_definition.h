// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LEVEL_DEFINITION_H
#define LEVEL_DEFINITION_H

#include "background_layer.h"
#include "content_values.h"
#include "hostile_scaling.h"

#include <string>
#include <vector>

namespace defn {

struct SpawnDefinition {
    double time = 0.0;
    std::string type = "jackal";
    // This one body's own multiple, on top of whatever its wave carries. Identity for every authored spawn; endless
    // uses it to promote a share of each wave to elites, which is a property of the body rather than of the wave.
    HostileScale scale;
};

struct WaveDefinition {
    int wave_number = 1;
    std::vector<SpawnDefinition> spawns;
    // What every hostile in this wave is multiplied by. Authored levels leave it at identity and endless raises it
    // per wave. It rides on the wave rather than being set on the scheduler between waves because a wave's spawns
    // and its wave-changed signal arrive in the same tick -- a setter would apply the new scale to some of the
    // wave's own spawns and the old one to the rest.
    HostileScale scale;
};

struct LevelDefinition {
    int level_id = 0;
    std::string name;
    int starting_core_resource = 100;
    int base_integrity = 3;
    // Both zero -- uncapped -- for every authored campaign level. Endless sets them, and they are level properties
    // rather than endless ones because they are ordinary match rules: any level or mode may want either, including
    // one with no base at all.
    int energy_cap = 0;
    int supply_cap = 0;
    Vector2 base_position_ratio{.x = 0.0760416667F, .y = 0.7407407407F};
    Vector2 belt_width_ratio{.x = 0.6944444444F, .y = 0.7870370370F};
    std::string background_path;
    /// Empty for a level drawn from one image. When present it replaces `background_path` entirely, and the
    /// layers are drawn back to front in the order given.
    std::vector<BackgroundLayer> background_layers;
    std::vector<WaveDefinition> waves;
};

} // namespace defn

#endif
