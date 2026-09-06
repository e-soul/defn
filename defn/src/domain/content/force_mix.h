// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef FORCE_MIX_H
#define FORCE_MIX_H

#include <span>
#include <string>
#include <vector>

namespace defn {

// One entry of a force: a unit id and how many of them stand on the belt.
struct MixEntry {
    std::string unit_id;
    int count = 0;
};

// A force with explicit counts. This is what the lab actually spawns and what a generated wave is built from.
using ForceMix = std::vector<MixEntry>;

// A force described by *shape* rather than size: relative weights that a budget is spent along. Two mixes with the
// same shape are the same strategy played at different budgets, which is the axis `critical_budget` bisects and the
// axis endless mode drifts along.
struct MixWeight {
    std::string unit_id;
    double weight = 0.0;
};

using MixShape = std::vector<MixWeight>;

// What one unit costs, in whatever currency the caller is spending. The lab spends energy, where a cost is the
// integer out of the catalog; endless spends measured threat, where a cost is a ratio against `grime` and every
// hostile's catalog cost is zero.
struct UnitCost {
    std::string unit_id;
    double cost = 0.0;
};

struct BudgetAllocation {
    ForceMix mix;
    // What the mix actually cost, in the caller's currency. Integral whenever the costs were.
    double budget_spent = 0.0;
    int energy_spent = 0;
};

// The order a mix takes the field in. Entries are interleaved round-robin rather than concatenated, so a 2:1 mix
// does not silently become "the first-listed unit is the whole front line" -- in the lab placement decides who
// trades first, and in a generated wave a concatenated order would arrive in blocks rather than as a mixed stream.
[[nodiscard]] std::vector<std::string> expand_mix(const ForceMix &mix);

[[nodiscard]] ForceMix mono_mix(std::string unit_id, int count);

[[nodiscard]] int total_units(const ForceMix &mix);

// Spends `budget` along `shape`. Largest-remainder allocation, not naive flooring: flooring collapses a 2:1 mix into
// a mono-stack at small budgets, which would make every low-budget probe of the bisection measure the wrong thing
// and would make an early endless wave the wrong composition rather than a small one.
[[nodiscard]] BudgetAllocation allocate_budget(std::span<const UnitCost> costs, const MixShape &shape, double budget);

} // namespace defn

#endif
