// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SIM_SCENARIO_H
#define SIM_SCENARIO_H

#include "policies/sim_policies.h"
#include "sim_camera.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace defn {

struct SimPolicySpec {
    // "scripted", "greedy", "defensive", "patience" or "mix".
    std::string kind = "greedy";
    std::vector<ScriptedCommand> script;
    int energy_reserve = 15;
    // "mix" only: the composition to play, as relative weights per unit id.
    std::map<std::string, double> weights;
    // What this policy is called in a sweep record, defaulting to `kind`. A scenario may run two policies of the
    // same kind -- two mixes, say -- and `aggregate_sim.py` groups by this name, so without it the two are silently
    // pooled into one row and the comparison the second one was added for cannot be read.
    std::string label;
};

// Everything a run needs, and everything it is reproducible from. `(scenario, seed)` decides the outcome completely.
struct SimScenario {
    std::string level_id = "level_01";
    // Where `level_id` is resolved from, empty for the shipped campaign. The tempo lab points this at its own
    // synthetic engagements so that a roster measurement never depends on content written to tell a story.
    std::string level_directory;
    std::uint32_t seed = 0;
    SimPolicySpec policy;
    std::vector<std::string> owned_upgrades;
    SimCameraMode camera = SimCameraMode::MODELLED;
    double max_seconds = 300.0;
    // World width comes from the background texture in the shipped game, so a Godot-hosted driver measures it and
    // supplies it here. Left empty, the kernel falls back to the viewport-derived default.
    std::optional<float> world_width;
};

[[nodiscard]] std::unique_ptr<PlayerPolicy> make_policy(const SimPolicySpec &spec);

} // namespace defn

#endif
