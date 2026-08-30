// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "sim_policies.h"

#include <algorithm>
#include <limits>
#include <map>
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

// Enough of the belt occupied that banking is a bet rather than a plan. Named, because two policies read it and a
// silent disagreement between them would look like a roster result.
constexpr int BUSY_BELT_HOSTILES = 3;

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

// Something has got through and the base is shooting at it. This is the narrow test: it says the line has already
// failed, not that a fight is happening.
bool hostile_at_the_gate(const MatchObservation &observation) { return closest_hostile_x(observation.entities) <= observation.base_engage_x; }

// The broad test, adding "enough is on the belt that the next few seconds are decided". Fine for a policy choosing
// *when* to spend a fixed ladder; ruinous for one choosing *what* to buy, because on a level of forty spawns it is
// true almost permanently -- see MixPolicy below.
bool under_pressure(const MatchObservation &observation) {
    return hostile_at_the_gate(observation) || count_live_hostiles(observation.entities) >= BUSY_BELT_HOSTILES;
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

std::vector<PlayerCommand> PatiencePolicy::decide(const MatchObservation &observation) {
    const UnitConfig *top = most_expensive(observation.roster);
    if (top == nullptr) {
        return {};
    }

    // Anything the base is already shooting at is a problem now; spend rather than bank.
    if (under_pressure(observation)) {
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

// The share each unit already holds of the field, so a target weight can be compared against something.
std::vector<PlayerCommand> MixPolicy::decide(const MatchObservation &observation) {
    double total_weight = 0.0;
    for (const auto &[unit_id, weight] : weights_) {
        total_weight += std::max(weight, 0.0);
    }
    if (total_weight <= 0.0) {
        return {};
    }

    std::map<std::string, int> on_field;
    int total_on_field = 0;
    for (const SimEntity &entity : observation.entities) {
        if (entity.dead || entity.side != UnitSide::FRIENDLY || !weights_.contains(entity.unit_id)) {
            continue;
        }
        ++on_field[entity.unit_id];
        ++total_on_field;
    }

    const auto field_size = static_cast<double>(std::max(total_on_field, 1));
    // Affordability is deliberately not part of the choice. Skipping to the next-neediest unit whenever the neediest
    // one is out of reach means the cheap end of the mix is bought every time energy crosses its cost, and the
    // expensive end never is -- the policy would play a mono-stack while claiming to play a composition.
    const UnitConfig *choice = nullptr;
    std::pair<double, int> best_key = {-std::numeric_limits<double>::max(), 0};
    // The same ranking restricted to what the purse can cover, kept only for the emergency below.
    const UnitConfig *affordable = nullptr;
    std::pair<double, int> best_affordable_key = {-std::numeric_limits<double>::max(), 0};
    for (const UnitConfig &unit : observation.roster) {
        const auto target = weights_.find(unit.name);
        if (target == weights_.end() || target->second <= 0.0) {
            continue;
        }

        const auto held = on_field.find(unit.name);
        const double share = held == on_field.end() ? 0.0 : static_cast<double>(held->second) / field_size;
        // Ties go to the more expensive unit, so a mix that is already on shape still spends rather than banking.
        const std::pair<double, int> key = {(target->second / total_weight) - share, unit.cost};
        if (choice == nullptr || key > best_key) {
            best_key = key;
            choice = &unit;
        }
        if (unit.cost <= observation.energy && (affordable == nullptr || key > best_affordable_key)) {
            best_affordable_key = key;
            affordable = &unit;
        }
    }

    if (choice != nullptr && choice->cost <= observation.energy) {
        return {PlayerCommand::deploy(choice->name)};
    }

    // Banking toward the unit the shape is short of is the whole reason this plays a composition rather than a
    // mono-stack of the cheap end, so it stays the default -- but a plan is worth nothing to a base that is being
    // hit while the purse fills. With something already at the gate, buy the neediest unit the mix names *and* can
    // afford.
    //
    // Without any exception the policy stalls exactly where a level is decided. On `level_01`,
    // `{breacher 2, marksman 1}` buys one breacher and then sits on 24 energy waiting for a 27-cost marksman while
    // four hounds cross the belt at 120px/s from t=3s: 94 energy spent against ~200 for every other policy, and 3
    // wins in 25.
    //
    // The test is deliberately the *narrow* one. `under_pressure`, which `patience` uses, also fires on a busy
    // belt -- and a busy belt is the normal condition of levels 3 to 5, so the exception would swallow the rule and
    // hand back the mono-stack: measured at marksman deployments 4.5 -> 1.0 per run on `level_04`, and 24/25 -> 0/25.
    if (affordable != nullptr && hostile_at_the_gate(observation)) {
        return {PlayerCommand::deploy(affordable->name)};
    }

    return {}; // bank toward the unit the shape is short of
}

} // namespace defn
