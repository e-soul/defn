// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "force_mix.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <utility>

namespace defn {

namespace {

const UnitCost *find_cost(std::span<const UnitCost> costs, const std::string &unit_id) {
    const auto found = std::ranges::find_if(costs, [&unit_id](const UnitCost &entry) { return entry.unit_id == unit_id; });
    return found == costs.end() ? nullptr : &*found;
}

} // namespace

std::vector<std::string> expand_mix(const ForceMix &mix) {
    std::vector<std::string> line;
    line.reserve(static_cast<std::size_t>(total_units(mix)));

    std::vector<int> remaining;
    remaining.reserve(mix.size());
    for (const MixEntry &entry : mix) {
        remaining.push_back(std::max(entry.count, 0));
    }

    bool placed_any = true;
    while (placed_any) {
        placed_any = false;
        for (std::size_t index = 0; index < mix.size(); ++index) {
            if (remaining[index] <= 0) {
                continue;
            }
            line.push_back(mix[index].unit_id);
            --remaining[index];
            placed_any = true;
        }
    }

    return line;
}

ForceMix mono_mix(std::string unit_id, int count) { return {{.unit_id = std::move(unit_id), .count = count}}; }

int total_units(const ForceMix &mix) {
    int count = 0;
    for (const MixEntry &entry : mix) {
        count += std::max(entry.count, 0);
    }
    return count;
}

BudgetAllocation allocate_budget(std::span<const UnitCost> costs, const MixShape &shape, double budget) {
    struct Slot {
        std::string unit_id;
        double cost = 0.0;
        double weight = 0.0;
        int count = 0;
        double remainder = 0.0;
    };

    std::vector<Slot> slots;
    double total_weight = 0.0;
    for (const MixWeight &entry : shape) {
        const UnitCost *cost = find_cost(costs, entry.unit_id);
        if (cost == nullptr || cost->cost <= 0.0 || entry.weight <= 0.0) {
            continue;
        }
        slots.push_back({.unit_id = entry.unit_id, .cost = cost->cost, .weight = entry.weight});
        total_weight += entry.weight;
    }

    if (slots.empty() || total_weight <= 0.0 || budget <= 0.0) {
        return {};
    }

    double spent = 0.0;
    for (Slot &slot : slots) {
        const double exact = budget * slot.weight / total_weight / slot.cost;
        slot.count = static_cast<int>(std::floor(exact));
        slot.remainder = exact - static_cast<double>(slot.count);
        spent += static_cast<double>(slot.count) * slot.cost;
    }

    // Largest remainder: whatever the flooring left unspent goes to the slots that were rounded down hardest, which
    // is what keeps the shape recognisable when the budget only buys a handful of units.
    //
    // The walk stops at the first slot that no longer fits rather than skipping over it to a cheaper one. Skipping
    // would spend the last of the budget on whichever unit happens to be cheapest, which drifts the shape at exactly
    // the budgets where the shape matters most; leaving under one unit's worth of budget unspent instead means B* is
    // the true smallest budget that buys the winning line.
    std::vector<std::size_t> order(slots.size());
    std::ranges::iota(order, std::size_t{0});
    std::ranges::stable_sort(order, [&slots](std::size_t left, std::size_t right) { return slots[left].remainder > slots[right].remainder; });
    for (const std::size_t index : order) {
        if (spent + slots[index].cost > budget) {
            break;
        }
        ++slots[index].count;
        spent += slots[index].cost;
    }

    BudgetAllocation allocation;
    allocation.budget_spent = spent;
    allocation.energy_spent = static_cast<int>(std::llround(spent));
    for (const Slot &slot : slots) {
        if (slot.count > 0) {
            allocation.mix.push_back({.unit_id = slot.unit_id, .count = slot.count});
        }
    }
    return allocation;
}

} // namespace defn
