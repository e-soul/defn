// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "sim_engagement_lab.h"

#include "random_source.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <utility>

namespace defn {

namespace {

// A force is identified by what it spawns, in order. Two budgets that buy the same line are the same experiment, and
// the bisection probes enough neighbouring budgets that not noticing would cost most of the run time.
std::string force_key(const std::vector<std::string> &line) {
    std::string key;
    for (const std::string &unit_id : line) {
        key += unit_id;
        key += '|';
    }
    return key;
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

EngagementOutcome run_engagement_once(const UnitCatalog &catalog, const GlobalUnitConfig &globals, const ForceMix &friendlies, const ForceMix &hostiles,
                                      std::uint32_t seed, const LabSetup &setup) {
    StdRandomSource random(seed);
    SimWorld world(catalog, globals, random);

    // Deliberately a second stream. Drawing the spawn band from the world's own source would shift every range
    // variation that follows it, so a measurement could not tell "the belt has depth now" from "the dice moved".
    StdRandomSource layout(seed ^ 0x9E3779B9U);
    const auto belt_y = [&layout, &setup]() { return layout.range_real(setup.belt_top_y, setup.belt_bottom_y); };

    float friendly_x = setup.friendly_front_x;
    for (const std::string &unit_id : expand_mix(friendlies)) {
        world.spawn(unit_id, UnitSide::FRIENDLY, {.x = friendly_x, .y = belt_y()});
        friendly_x -= setup.friendly_spacing;
    }

    float hostile_x = setup.hostile_front_x;
    for (const std::string &unit_id : expand_mix(hostiles)) {
        world.spawn(unit_id, UnitSide::HOSTILE, {.x = hostile_x, .y = belt_y()});
        hostile_x += setup.hostile_spacing;
    }
    world.begin_run();

    const SimEngagementReport report = run_engagement(world, setup.max_seconds);

    EngagementOutcome outcome;
    outcome.friendly_won = report.winner.has_value() && *report.winner == UnitSide::FRIENDLY;
    outcome.duration_seconds = report.duration_seconds;
    for (const SimEntity &entity : world.get_entities()) {
        if (entity.side == UnitSide::FRIENDLY) {
            outcome.friendly_damage_taken += entity.damage_taken;
            outcome.friendlies_lost += entity.dead ? 1 : 0;
        } else if (entity.dead) {
            ++outcome.hostiles_killed;
        }
    }

    return outcome;
}

AveragedEngagement average_engagement(const UnitCatalog &catalog, const GlobalUnitConfig &globals, const ForceMix &friendlies, const ForceMix &hostiles,
                                      std::span<const std::uint32_t> seeds, const LabSetup &setup) {
    AveragedEngagement total;
    for (const std::uint32_t seed : seeds) {
        const EngagementOutcome outcome = run_engagement_once(catalog, globals, friendlies, hostiles, seed, setup);
        total.win_rate += outcome.friendly_won ? 1.0 : 0.0;
        total.duration_seconds += outcome.duration_seconds;
        total.friendly_damage_taken += outcome.friendly_damage_taken;
        total.hostiles_killed += outcome.hostiles_killed;
        total.friendlies_lost += outcome.friendlies_lost;
    }

    const auto count = static_cast<double>(std::max<std::size_t>(seeds.size(), 1));
    total.win_rate /= count;
    total.duration_seconds /= count;
    total.friendly_damage_taken /= count;
    total.hostiles_killed /= count;
    total.friendlies_lost /= count;
    return total;
}

std::vector<std::uint32_t> default_seeds(int count) {
    std::vector<std::uint32_t> seeds;
    seeds.reserve(static_cast<std::size_t>(std::max(count, 0)));
    for (int index = 0; index < count; ++index) {
        seeds.push_back(static_cast<std::uint32_t>(2026 + index));
    }
    return seeds;
}

BudgetAllocation allocate_budget(const UnitCatalog &catalog, const MixShape &shape, double budget) {
    struct Slot {
        std::string unit_id;
        int cost = 0;
        double weight = 0.0;
        int count = 0;
        double remainder = 0.0;
    };

    std::vector<Slot> slots;
    double total_weight = 0.0;
    for (const MixWeight &entry : shape) {
        const auto config = catalog.get_unit(entry.unit_id);
        if (!config.has_value() || config->cost <= 0 || entry.weight <= 0.0) {
            continue;
        }
        slots.push_back({.unit_id = entry.unit_id, .cost = config->cost, .weight = entry.weight});
        total_weight += entry.weight;
    }

    if (slots.empty() || total_weight <= 0.0 || budget <= 0.0) {
        return {};
    }

    int spent = 0;
    for (Slot &slot : slots) {
        const double exact = budget * slot.weight / total_weight / static_cast<double>(slot.cost);
        slot.count = static_cast<int>(std::floor(exact));
        slot.remainder = exact - static_cast<double>(slot.count);
        spent += slot.count * slot.cost;
    }

    // Largest remainder: whatever the flooring left unspent goes to the slots that were rounded down hardest, which
    // is what keeps the shape recognisable when the budget only buys a handful of units.
    //
    // The walk stops at the first slot that no longer fits rather than skipping over it to a cheaper one. Skipping
    // would spend the last of the budget on whichever unit happens to be cheapest, which drifts the shape at exactly
    // the budgets where the shape matters most; leaving under one unit's worth of energy unspent instead means B* is
    // the true smallest budget that buys the winning line.
    std::vector<std::size_t> order(slots.size());
    std::ranges::iota(order, 0U);
    std::ranges::stable_sort(order, [&slots](std::size_t left, std::size_t right) { return slots[left].remainder > slots[right].remainder; });
    for (const std::size_t index : order) {
        if (static_cast<double>(spent + slots[index].cost) > budget) {
            break;
        }
        ++slots[index].count;
        spent += slots[index].cost;
    }

    BudgetAllocation allocation;
    allocation.energy_spent = spent;
    for (const Slot &slot : slots) {
        if (slot.count > 0) {
            allocation.mix.push_back({.unit_id = slot.unit_id, .count = slot.count});
        }
    }
    return allocation;
}

CriticalBudget critical_budget(const UnitCatalog &catalog, const GlobalUnitConfig &globals, const MixShape &friendly_shape, const ForceMix &hostiles,
                               std::span<const std::uint32_t> seeds, const CriticalBudgetOptions &options, const LabSetup &setup) {
    CriticalBudget result;

    std::map<std::string, AveragedEngagement> measured;
    const auto probe = [&](double budget) {
        const BudgetAllocation allocation = allocate_budget(catalog, friendly_shape, budget);
        const std::string key = force_key(expand_mix(allocation.mix));
        const auto cached = measured.find(key);
        if (cached == measured.end()) {
            ++result.probes;
            const AveragedEngagement measurement = average_engagement(catalog, globals, allocation.mix, hostiles, seeds, setup);
            measured.emplace(key, measurement);
            return std::pair{allocation, measurement};
        }
        return std::pair{allocation, cached->second};
    };

    const auto record = [&result](const BudgetAllocation &allocation, const AveragedEngagement &measurement, double budget) {
        result.bounded = true;
        result.energy = budget;
        result.bought = allocation.mix;
        result.energy_spent = allocation.energy_spent;
        result.win_rate = measurement.win_rate;
        result.duration_seconds = measurement.duration_seconds;
        result.friendly_damage_taken = measurement.friendly_damage_taken;
        result.hostiles_killed = measurement.hostiles_killed;
        result.friendlies_lost = measurement.friendlies_lost;
    };

    const auto [ceiling_allocation, ceiling_measurement] = probe(options.max_budget);
    if (ceiling_measurement.win_rate < options.win_threshold) {
        result.energy = options.max_budget;
        result.bought = ceiling_allocation.mix;
        result.energy_spent = ceiling_allocation.energy_spent;
        result.win_rate = ceiling_measurement.win_rate;
        result.duration_seconds = ceiling_measurement.duration_seconds;
        result.friendly_damage_taken = ceiling_measurement.friendly_damage_taken;
        result.hostiles_killed = ceiling_measurement.hostiles_killed;
        result.friendlies_lost = ceiling_measurement.friendlies_lost;
        return result;
    }
    record(ceiling_allocation, ceiling_measurement, options.max_budget);

    double low = 0.0;
    double high = options.max_budget;
    for (int iteration = 0; iteration < options.max_iterations && high - low > options.tolerance; ++iteration) {
        const double middle = 0.5 * (low + high);
        const auto [allocation, measurement] = probe(middle);
        if (measurement.win_rate >= options.win_threshold) {
            high = middle;
            record(allocation, measurement, middle);
        } else {
            low = middle;
        }
    }

    return result;
}

} // namespace defn
