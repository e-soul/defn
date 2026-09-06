// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SIM_ENGAGEMENT_LAB_H
#define SIM_ENGAGEMENT_LAB_H

#include "force_mix.h"
#include "sim_world.h"
#include "unit_definition.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace defn {

// Where the two lines stand and how long the lab waits for a decision. Fixed for every measurement, because the only
// thing that may differ between cells is the compositions.
struct LabSetup {
    // The belt is a band, not a line, and units are dropped anywhere across it -- the same rule the real game uses
    // in GridManager::sample_belt_y, and the same default numbers as GameplayRules. It matters for exactly one
    // mechanic: splash is the only rule in the game that reads a 2-D distance, so a lab that stood every unit on
    // one line was the only place where an area weapon could not miss.
    float belt_top_y = 750.0F;
    float belt_bottom_y = 850.0F;
    float friendly_front_x = 800.0F;
    // Friendlies fill backwards from the front, hostiles forwards from theirs: the two lines face each other.
    //
    // Spacing must stay **wider than the largest melee reach in the game** (100px), or the lab has no notion of a
    // back rank: a unit arriving at the front rank finds the second rank already in contact, so anything that means
    // to walk past the first to reach the second has nothing to walk past. The original 70px was chosen for a purely
    // ranged, 1-D roster and silently measured every positional mechanic as a flat null. See `BALANCE_TOOLING.md`.
    float friendly_spacing = 200.0F;
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

[[nodiscard]] EngagementOutcome run_engagement_once(const UnitCatalog &catalog, const GlobalUnitConfig &globals, const ForceMix &friendlies,
                                                    const ForceMix &hostiles, std::uint32_t seed, const LabSetup &setup = {});

[[nodiscard]] AveragedEngagement average_engagement(const UnitCatalog &catalog, const GlobalUnitConfig &globals, const ForceMix &friendlies,
                                                    const ForceMix &hostiles, std::span<const std::uint32_t> seeds, const LabSetup &setup = {});

// Seeds `2026, 2027, ...`, which is what the balance tables have always averaged over.
[[nodiscard]] std::vector<std::uint32_t> default_seeds(int count);

// Spends `budget` along `shape`, pricing each unit at its catalog energy cost. A thin reading of the catalog on top
// of the generalised apportionment in `force_mix.h`; units the catalog does not price are skipped, which is why a
// hostile shape -- every hostile carries `bounty` and `cost: 0` -- has to be priced through the span overload.
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
