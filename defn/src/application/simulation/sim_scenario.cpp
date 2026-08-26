// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "sim_scenario.h"

namespace defn {

std::unique_ptr<PlayerPolicy> make_policy(const SimPolicySpec &spec) {
    if (spec.kind == "scripted") {
        return std::make_unique<ScriptedPolicy>(spec.script);
    }
    if (spec.kind == "defensive") {
        return std::make_unique<DefensivePolicy>();
    }
    if (spec.kind == "patience") {
        return std::make_unique<PatiencePolicy>(spec.energy_reserve);
    }
    if (spec.kind == "mix") {
        return std::make_unique<MixPolicy>(spec.weights);
    }

    return std::make_unique<GreedyPolicy>();
}

} // namespace defn
