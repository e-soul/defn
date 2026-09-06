// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "endless_schedule_loader.h"

#include "godot_string.h"
#include "json_file_loader.h"
#include "level_loader.h"
#include "variant_tools.h"

#include <godot_cpp/variant/array.hpp>

namespace defn {

namespace {

MixShape parse_weights(const Dictionary &weights) {
    MixShape shape;
    const Array unit_ids = weights.keys();
    shape.reserve(unit_ids.size());
    for (const Variant &unit_id : unit_ids) {
        shape.push_back({
            .unit_id = to_std_string(String(unit_id)),
            .weight = VariantTools::as_double(weights[unit_id]),
        });
    }
    return shape;
}

EndlessTuning parse_tuning(const Dictionary &data) {
    const EndlessTuning defaults;
    return {
        .base_budget = VariantTools::as_double(data.get("base_budget", defaults.base_budget)),
        .escalation = VariantTools::as_double(data.get("escalation", defaults.escalation)),
        .escalation_curve = VariantTools::as_double(data.get("escalation_curve", defaults.escalation_curve)),
        .hostile_damage_growth = VariantTools::as_double(data.get("hostile_damage_growth", defaults.hostile_damage_growth)),
        .hostile_damage_cap = VariantTools::as_double(data.get("hostile_damage_cap", defaults.hostile_damage_cap)),
        .bounty_decay = VariantTools::as_double(data.get("bounty_decay", defaults.bounty_decay)),
        .first_wave_delay = VariantTools::as_double(data.get("first_wave_delay", defaults.first_wave_delay)),
        .wave_interval = VariantTools::as_double(data.get("wave_interval", defaults.wave_interval)),
        .interval_growth = VariantTools::as_double(data.get("interval_growth", defaults.interval_growth)),
        .spawn_stagger = VariantTools::as_double(data.get("spawn_stagger", defaults.spawn_stagger)),
        .budget_ceiling = VariantTools::as_double(data.get("budget_ceiling", defaults.budget_ceiling)),
        .wall_clock_ceiling = VariantTools::as_double(data.get("wall_clock_ceiling", defaults.wall_clock_ceiling)),
        .survival_bonus_per_wave = VariantTools::as_int(data.get("survival_bonus_per_wave", defaults.survival_bonus_per_wave)),
    };
}

std::vector<UnitCost> parse_threat_costs(const Dictionary &costs) {
    std::vector<UnitCost> result;
    const Array unit_ids = costs.keys();
    result.reserve(unit_ids.size());
    for (const Variant &unit_id : unit_ids) {
        result.push_back({
            .unit_id = to_std_string(String(unit_id)),
            .cost = VariantTools::as_double(costs[unit_id]),
        });
    }
    return result;
}

std::vector<ShapeKeyframe> parse_drift(const Array &keyframes) {
    std::vector<ShapeKeyframe> result;
    result.reserve(keyframes.size());
    for (const Variant &value : keyframes) {
        if (value.get_type() != Variant::DICTIONARY) {
            continue;
        }
        const Dictionary keyframe = value;
        result.push_back({
            .wave = VariantTools::as_int(keyframe.get("wave", 1)),
            .weights = parse_weights(keyframe.get("weights", Dictionary())),
        });
    }
    return result;
}

std::vector<SetPiece> parse_set_pieces(const Array &set_pieces) {
    std::vector<SetPiece> result;
    result.reserve(set_pieces.size());
    for (const Variant &value : set_pieces) {
        if (value.get_type() != Variant::DICTIONARY) {
            continue;
        }
        const Dictionary set_piece = value;
        result.push_back({
            .period = VariantTools::as_int(set_piece.get("period", 0)),
            .offset = VariantTools::as_int(set_piece.get("offset", 0)),
            .weights = parse_weights(set_piece.get("weights", Dictionary())),
        });
    }
    return result;
}

} // namespace

std::optional<EndlessDefinition> EndlessScheduleLoader::load(const String &path) {
    const auto data = JsonFileLoader::load_dictionary(path, "EndlessScheduleLoader");
    return data ? load_from_data(*data) : std::nullopt;
}

std::optional<EndlessDefinition> EndlessScheduleLoader::load_from_data(const Dictionary &data) {
    EndlessDefinition definition;

    // The level travels through the same parser the campaign levels do, so a synthesised level cannot drift away
    // from what `LevelLoader` accepts. It simply carries no waves.
    const auto level = LevelLoader::load_from_data(data.get("level", Dictionary()));
    if (!level.has_value()) {
        return std::nullopt;
    }
    definition.level = *level;
    definition.level.waves.clear();

    definition.schedule.tuning = parse_tuning(data.get("tuning", Dictionary()));
    definition.schedule.threat_costs = parse_threat_costs(data.get("threat_costs", Dictionary()));
    definition.schedule.drift = parse_drift(data.get("drift", Array()));
    definition.schedule.set_pieces = parse_set_pieces(data.get("set_pieces", Array()));
    return definition;
}

} // namespace defn
