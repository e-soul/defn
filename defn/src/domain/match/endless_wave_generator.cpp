// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "endless_wave_generator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <string>
#include <utility>

namespace defn {

namespace {

// Spawns fill the front of a wave's interval and leave the tail quiet, so a wave reads as an arrival rather than as
// a continuous stream that happens to be numbered.
constexpr double SPAWN_WINDOW_FRACTION = 0.9;

// Jitter as a fraction of the stagger. Enough that two runs of the same wave do not arrive in lockstep, small
// enough that it never reorders the line past its neighbour by more than one slot.
constexpr double SPAWN_JITTER_FRACTION = 0.5;

// Income decays but never reaches zero: a long run has to stay a game rather than becoming a countdown.
constexpr double MINIMUM_BOUNTY_MULTIPLIER = 0.05;

double cost_of(std::span<const UnitCost> costs, const std::string &unit_id) {
    const auto found = std::ranges::find_if(costs, [&unit_id](const UnitCost &entry) { return entry.unit_id == unit_id; });
    return found == costs.end() ? 0.0 : found->cost;
}

double weight_for(const MixShape &shape, const std::string &unit_id) {
    const auto found = std::ranges::find_if(shape, [&unit_id](const MixWeight &entry) { return entry.unit_id == unit_id; });
    return found == shape.end() ? 0.0 : found->weight;
}

MixShape interpolate(const MixShape &from, const MixShape &into, double blend) {
    MixShape result;
    result.reserve(from.size() + into.size());
    for (const MixWeight &entry : from) {
        result.push_back({.unit_id = entry.unit_id, .weight = std::lerp(entry.weight, weight_for(into, entry.unit_id), blend)});
    }
    for (const MixWeight &entry : into) {
        const bool already_placed = std::ranges::any_of(result, [&entry](const MixWeight &placed) { return placed.unit_id == entry.unit_id; });
        if (!already_placed) {
            result.push_back({.unit_id = entry.unit_id, .weight = std::lerp(0.0, entry.weight, blend)});
        }
    }
    return result;
}

} // namespace

void EndlessWaveGenerator::configure(const EndlessSchedule &schedule) {
    schedule_ = schedule;
    std::ranges::stable_sort(schedule_.drift, [](const ShapeKeyframe &left, const ShapeKeyframe &right) { return left.wave < right.wave; });
}

double EndlessWaveGenerator::budget(int wave_number) const {
    const int index = std::max(wave_number, 1) - 1;
    // The curve is applied to the index rather than to the rate, so `a = 1.0` reproduces the geometric ramp exactly
    // and the opening waves stay where they were measured: at `a = 1.2` wave 5 is 38 threat against the geometric
    // 35, while wave 30 is 770 against 152. The mode is meant to be won early and lost late, so the divergence has
    // to sit late.
    const double curved = std::pow(static_cast<double>(index), schedule_.tuning.escalation_curve);
    return schedule_.tuning.base_budget * std::pow(schedule_.tuning.escalation, curved);
}

double EndlessWaveGenerator::wave_interval(int wave_number) const {
    const int index = std::max(wave_number, 1) - 1;
    return schedule_.tuning.wave_interval * std::pow(schedule_.tuning.interval_growth, static_cast<double>(index));
}

double EndlessWaveGenerator::wave_start_time(int wave_number) const {
    double start = schedule_.tuning.first_wave_delay;
    for (int wave = 1; wave < wave_number; ++wave) {
        start += wave_interval(wave);
    }
    return start;
}

MixShape EndlessWaveGenerator::shape(int wave_number) const {
    const int wave = std::max(wave_number, 1);
    for (const SetPiece &set_piece : schedule_.set_pieces) {
        if (set_piece.period > 0 && wave % set_piece.period == set_piece.offset % set_piece.period) {
            return set_piece.weights;
        }
    }

    if (schedule_.drift.empty()) {
        return {};
    }
    if (wave <= schedule_.drift.front().wave) {
        return schedule_.drift.front().weights;
    }
    if (wave >= schedule_.drift.back().wave) {
        return schedule_.drift.back().weights;
    }

    for (std::size_t index = 1; index < schedule_.drift.size(); ++index) {
        const ShapeKeyframe &into = schedule_.drift[index];
        if (wave > into.wave) {
            continue;
        }
        const ShapeKeyframe &from = schedule_.drift[index - 1];
        const int span = into.wave - from.wave;
        const double blend = span > 0 ? static_cast<double>(wave - from.wave) / static_cast<double>(span) : 1.0;
        return interpolate(from.weights, into.weights, blend);
    }

    return schedule_.drift.back().weights;
}

WaveDefinition EndlessWaveGenerator::generate(int wave_number, RandomSource &random) const {
    const int wave = std::max(wave_number, 1);
    WaveDefinition definition;
    definition.wave_number = wave;
    definition.damage_scale = hostile_damage_scale(wave);

    const std::span<const UnitCost> costs(schedule_.threat_costs);
    const BudgetAllocation allocation = allocate_budget(costs, to_budget_shape(shape(wave), costs), budget(wave));
    std::vector<std::string> line = expand_mix(allocation.mix);
    if (line.empty()) {
        return definition;
    }

    // Fisher-Yates over the round-robin line. Composition is already fixed by the budget and the shape; what varies
    // between two runs of the same wave is only who walks in first.
    for (std::size_t index = line.size(); index > 1; --index) {
        const auto swap_with = static_cast<std::size_t>(random.range_int(0, static_cast<int>(index) - 1));
        std::swap(line[index - 1], line[swap_with]);
    }

    const double start = wave_start_time(wave);
    const double window = wave_interval(wave) * SPAWN_WINDOW_FRACTION;
    const double even_stagger = line.size() > 1 ? window / static_cast<double>(line.size() - 1) : 0.0;
    // A wave that has outgrown its window compresses rather than spilling into the next one, which is what keeps the
    // late game a composition test instead of a spawn-rate test.
    const double stagger = std::min(schedule_.tuning.spawn_stagger, even_stagger);

    definition.spawns.reserve(line.size());
    for (std::size_t index = 0; index < line.size(); ++index) {
        const double jitter = random.range_real(0.0F, static_cast<float>(stagger * SPAWN_JITTER_FRACTION));
        const double time = std::clamp(start + (static_cast<double>(index) * stagger) + jitter, start, start + window);
        definition.spawns.push_back({.time = time, .type = line[index]});
    }

    std::ranges::stable_sort(definition.spawns, [](const SpawnDefinition &left, const SpawnDefinition &right) { return left.time < right.time; });
    return definition;
}

MixShape EndlessWaveGenerator::to_budget_shape(const MixShape &counts, std::span<const UnitCost> costs) {
    // A keyframe is authored the way a designer reads it -- "six grime to two hounds" is a ratio of *bodies*. The
    // apportionment spends along a ratio of *budget*, and a hound costs nearly five times what a grime does, so
    // handing it the authored numbers unconverted buys twelve grime and no hound and calls that six-to-two.
    MixShape budget_weights;
    budget_weights.reserve(counts.size());
    for (const MixWeight &entry : counts) {
        budget_weights.push_back({.unit_id = entry.unit_id, .weight = entry.weight * cost_of(costs, entry.unit_id)});
    }
    return budget_weights;
}

double EndlessWaveGenerator::hostile_damage_scale(int wave_number) const {
    const int index = std::max(wave_number, 1) - 1;
    const double grown = std::pow(schedule_.tuning.hostile_damage_growth, static_cast<double>(index));
    return std::clamp(grown, 1.0, std::max(1.0, schedule_.tuning.hostile_damage_cap));
}

double EndlessWaveGenerator::bounty_multiplier(int wave_number) const {
    const int index = std::max(wave_number, 1) - 1;
    const double decayed = std::pow(schedule_.tuning.bounty_decay, static_cast<double>(index));
    return std::max(decayed, MINIMUM_BOUNTY_MULTIPLIER);
}

} // namespace defn
