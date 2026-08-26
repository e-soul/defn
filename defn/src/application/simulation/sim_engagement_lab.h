// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SIM_ENGAGEMENT_LAB_H
#define SIM_ENGAGEMENT_LAB_H

#include "sim_world.h"
#include "unit_definition.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace defn {

// One entry of a force: a unit id and how many of them stand on the belt.
struct MixEntry {
    std::string unit_id;
    int count = 0;
};

// A force with explicit counts. This is what the lab actually spawns.
using ForceMix = std::vector<MixEntry>;

// A force described by *shape* rather than size: relative weights that an energy budget is spent along. Two mixes
// with the same shape are the same strategy played at different budgets, which is the axis `critical_budget` bisects.
struct MixWeight {
    std::string unit_id;
    double weight = 0.0;
};

using MixShape = std::vector<MixWeight>;

// Where the two lines stand and how long the lab waits for a decision. Fixed for every measurement, because the only
// thing that may differ between cells is the compositions.
struct LabSetup {
    float belt_y = 800.0F;
    float friendly_front_x = 800.0F;
    // Friendlies fill backwards from the front, hostiles forwards from theirs: the two lines face each other.
    float friendly_spacing = 70.0F;
    float hostile_front_x = 1600.0F;
    float hostile_spacing = 110.0F;
    double max_seconds = 180.0;
};

struct EngagementOutcome {
    bool friendly_won = false;
    double duration_seconds = 0.0;
    int friendly_damage_taken = 0;
    int hostiles_killed = 0;
    int friendlies_lost = 0;
};

struct AveragedEngagement {
    double win_rate = 0.0;
    double duration_seconds = 0.0;
    double friendly_damage_taken = 0.0;
    double hostiles_killed = 0.0;
    double friendlies_lost = 0.0;
};

// The order a mix takes the field in. Entries are interleaved round-robin rather than concatenated, so a 2:1 mix does
// not silently become "the first-listed unit is the whole front line" -- placement decides who trades first, and a
// concatenated order would measure the ordering rather than the composition.
[[nodiscard]] std::vector<std::string> expand_mix(const ForceMix &mix);

[[nodiscard]] ForceMix mono_mix(std::string unit_id, int count);

[[nodiscard]] int total_units(const ForceMix &mix);

[[nodiscard]] EngagementOutcome run_engagement_once(const UnitCatalog &catalog, const GlobalUnitConfig &globals, const ForceMix &friendlies,
                                                    const ForceMix &hostiles, std::uint32_t seed, const LabSetup &setup = {});

[[nodiscard]] AveragedEngagement average_engagement(const UnitCatalog &catalog, const GlobalUnitConfig &globals, const ForceMix &friendlies,
                                                    const ForceMix &hostiles, std::span<const std::uint32_t> seeds, const LabSetup &setup = {});

// Seeds `2026, 2027, ...`, which is what the balance tables have always averaged over.
[[nodiscard]] std::vector<std::uint32_t> default_seeds(int count);

struct BudgetAllocation {
    ForceMix mix;
    int energy_spent = 0;
};

// Spends `budget` along `shape`. Largest-remainder allocation, not naive flooring: flooring collapses a 2:1 mix into
// a mono-stack at small budgets, which would make every low-budget probe of the bisection measure the wrong thing.
[[nodiscard]] BudgetAllocation allocate_budget(const UnitCatalog &catalog, const MixShape &shape, double budget);

struct CriticalBudgetOptions {
    // The ceiling the bisection searches under. A cell that still loses here is reported unbounded rather than given
    // a bogus large number.
    double max_budget = 400.0;
    double tolerance = 2.0;
    int max_iterations = 8;
    double win_threshold = 0.5;
};

// The smallest energy budget at which a shape beats a force, and what that budget bought.
//
// Win rate saturates -- every advanced cell reads 100% -- and a saturated scale cannot rank anything. Budget never
// saturates, is denominated in the same energy the player spends, and `log B*` is approximately additive, which is
// what makes decomposing a matrix of these numbers mean something.
struct CriticalBudget {
    // False when even `max_budget` loses. `energy` is then `max_budget` and carries no information.
    bool bounded = false;
    double energy = 0.0;
    // What `energy` actually bought, and what the reported outcome fields were measured on.
    ForceMix bought;
    int energy_spent = 0;
    double win_rate = 0.0;
    double duration_seconds = 0.0;
    double friendly_damage_taken = 0.0;
    double hostiles_killed = 0.0;
    double friendlies_lost = 0.0;
    // How many budgets were probed, so a cell that ran out of iterations rather than converging is visible.
    int probes = 0;
};

[[nodiscard]] CriticalBudget critical_budget(const UnitCatalog &catalog, const GlobalUnitConfig &globals, const MixShape &friendly_shape,
                                             const ForceMix &hostiles, std::span<const std::uint32_t> seeds, const CriticalBudgetOptions &options = {},
                                             const LabSetup &setup = {});

} // namespace defn

#endif
