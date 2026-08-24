// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "sim_policies.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace defn {

namespace {

// The most expensive affordable entry, which on this roster is also the strongest.
const UnitConfig *best_affordable(std::span<const UnitConfig> roster, int energy) {
    const UnitConfig *best = nullptr;
    for (const UnitConfig &unit : roster) {
        if (unit.cost > energy) {
            continue;
        }
        if (best == nullptr || unit.cost > best->cost) {
            best = &unit;
        }
    }

    return best;
}

const UnitConfig *most_expensive(std::span<const UnitConfig> roster) {
    const UnitConfig *best = nullptr;
    for (const UnitConfig &unit : roster) {
        if (best == nullptr || unit.cost > best->cost) {
            best = &unit;
        }
    }

    return best;
}

int count_live_hostiles(std::span<const SimEntity> entities) {
    int count = 0;
    for (const SimEntity &entity : entities) {
        if (!entity.dead && entity.side == UnitSide::HOSTILE) {
            ++count;
        }
    }

    return count;
}

// How far the closest live hostile has pushed toward the base, in world x. Lower means more urgent.
float closest_hostile_x(std::span<const SimEntity> entities) {
    float closest = std::numeric_limits<float>::max();
    for (const SimEntity &entity : entities) {
        if (!entity.dead && entity.side == UnitSide::HOSTILE) {
            closest = std::min(closest, entity.position.x);
        }
    }

    return closest;
}

} // namespace

ScriptedPolicy::ScriptedPolicy(std::vector<ScriptedCommand> script) : script_(std::move(script)) {
    std::ranges::stable_sort(script_, [](const ScriptedCommand &left, const ScriptedCommand &right) { return left.time_seconds < right.time_seconds; });
}

std::vector<PlayerCommand> ScriptedPolicy::decide(const MatchObservation &observation) {
    std::vector<PlayerCommand> commands;
    while (next_index_ < script_.size() && script_[next_index_].time_seconds <= observation.elapsed_seconds) {
        commands.push_back(PlayerCommand::deploy(script_[next_index_].unit_id));
        ++next_index_;
    }

    return commands;
}

std::vector<PlayerCommand> GreedyPolicy::decide(const MatchObservation &observation) {
    const UnitConfig *choice = best_affordable(observation.roster, observation.energy);
    if (choice == nullptr) {
        return {};
    }

    return {PlayerCommand::deploy(choice->name)};
}

std::vector<PlayerCommand> DefensivePolicy::decide(const MatchObservation &observation) {
    if (!committed_) {
        if (closest_hostile_x(observation.entities) > observation.base_engage_x) {
            return {}; // still holding: bank the energy and let them come
        }
        committed_ = true;
    }

    const UnitConfig *choice = best_affordable(observation.roster, observation.energy);
    if (choice == nullptr) {
        return {};
    }

    return {PlayerCommand::deploy(choice->name)};
}

std::vector<PlayerCommand> CompositionPolicy::decide(const MatchObservation &observation) {
    const UnitConfig *top = most_expensive(observation.roster);
    if (top == nullptr) {
        return {};
    }

    // Anything the base is already shooting at is a problem now; spend rather than bank.
    const bool under_pressure = closest_hostile_x(observation.entities) <= observation.base_engage_x;
    const bool belt_is_busy = count_live_hostiles(observation.entities) >= 3;
    if (under_pressure || belt_is_busy) {
        if (const UnitConfig *choice = best_affordable(observation.roster, observation.energy); choice != nullptr) {
            return {PlayerCommand::deploy(choice->name)};
        }
        return {};
    }

    // Otherwise save toward the top of the roster, keeping a reserve so an emergency is still answerable.
    if (observation.energy >= top->cost + energy_reserve_) {
        return {PlayerCommand::deploy(top->name)};
    }

    return {};
}

} // namespace defn
