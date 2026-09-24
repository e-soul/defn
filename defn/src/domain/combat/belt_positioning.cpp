// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "belt_positioning.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace defn {

namespace {

float band_for(const BeltUnitSnapshot &unit) {
    if (unit.attack_mode == AttackMode::RANGED) {
        const float distance = std::abs(unit.position.x - unit.approach_position.x);
        return std::min(unit.config.ranged_base_tolerance + (distance * unit.config.ranged_angle_slope), unit.config.ranged_max_tolerance);
    }
    return unit.config.melee_band;
}

float slot_y(const BeltUnitSnapshot &unit, int slot, float top, float bottom) {
    const float gap = std::max(unit.config.desired_gap, unit.config.minimum_gap);
    const float offset = std::clamp(static_cast<float>(slot) * gap, -band_for(unit), band_for(unit));
    return std::clamp(unit.approach_position.y + offset, top + unit.config.edge_inset, bottom - unit.config.edge_inset);
}

bool same_group(const BeltUnitSnapshot &left, const BeltUnitSnapshot &right) {
    return left.side == right.side && left.approach_id.is_valid() && left.approach_id == right.approach_id;
}

float separation_from(const BeltUnitSnapshot &unit, const BeltUnitSnapshot &other) {
    if (other.id == unit.id || other.dead || other.side != unit.side) {
        return 0.0F;
    }
    const float width = unit.config.footprint_half_width + other.config.footprint_half_width;
    const float depth = unit.config.footprint_half_depth + other.config.footprint_half_depth;
    const float distance_x = std::abs(unit.position.x - other.position.x);
    const float distance_y = unit.position.y - other.position.y;
    if (distance_x >= width || depth <= 0.0F) {
        return 0.0F;
    }
    const float ellipse_height = depth * std::sqrt(std::max(0.0F, 1.0F - ((distance_x * distance_x) / (width * width))));
    const float overlap = ellipse_height - std::abs(distance_y) - unit.config.overlap_dead_zone;
    if (overlap <= 0.0F) {
        return 0.0F;
    }
    float direction = unit.id.value < other.id.value ? -1.0F : 1.0F;
    if (distance_y != 0.0F) {
        direction = std::copysign(1.0F, distance_y);
    }
    return direction * overlap;
}

} // namespace

int BeltPositioning::choose_slot(const BeltUnitSnapshot &unit, std::span<const BeltUnitSnapshot> units, float top, float bottom) const {
    const float gap = std::max(unit.config.desired_gap, unit.config.minimum_gap);
    const int radius = std::max(0, static_cast<int>(band_for(unit) / std::max(gap, 1.0F)));
    float best_score = std::numeric_limits<float>::max();
    int best_slot = 0;
    for (int slot = -radius; slot <= radius; ++slot) {
        int occupants = 0;
        for (const BeltUnitSnapshot &other : units) {
            if (other.id == unit.id || !same_group(unit, other)) {
                continue;
            }
            const auto found = states_.find(other.id.value);
            if (found != states_.end() && found->second.assigned && found->second.slot == slot) {
                ++occupants;
            }
        }
        const float score = (static_cast<float>(occupants) * 10000.0F) + std::abs(slot_y(unit, slot, top, bottom) - unit.position.y);
        if (score < best_score) {
            best_score = score;
            best_slot = slot;
        }
    }
    return best_slot;
}

void BeltPositioning::retain_assignments(std::span<const BeltUnitSnapshot> units, float top, float bottom) {
    std::erase_if(states_, [&units](const auto &entry) {
        return std::ranges::none_of(units, [&entry](const BeltUnitSnapshot &unit) { return unit.id.value == entry.first && !unit.dead; });
    });
    for (const BeltUnitSnapshot &unit : units) {
        if (unit.dead || !unit.approach_id.is_valid()) {
            continue;
        }
        State &state = states_[unit.id.value];
        if (state.target != unit.approach_id) {
            state.target = unit.approach_id;
            state.assigned = false;
            state.velocity = 0.0F;
        }
    }
    for (const BeltUnitSnapshot &unit : units) {
        if (unit.dead || !unit.approach_id.is_valid()) {
            continue;
        }
        State &state = states_[unit.id.value];
        if (!state.assigned) {
            state.slot = choose_slot(unit, units, top, bottom);
            state.assigned = true;
        } else {
            const float current_distance = std::abs(slot_y(unit, state.slot, top, bottom) - unit.position.y);
            if (current_distance > band_for(unit) + unit.config.reassignment_hysteresis) {
                const int alternative = choose_slot(unit, units, top, bottom);
                const float alternative_distance = std::abs(slot_y(unit, alternative, top, bottom) - unit.position.y);
                if (current_distance > alternative_distance + unit.config.reassignment_hysteresis) {
                    state.slot = alternative;
                }
            }
        }
    }
}

BeltPositionResult BeltPositioning::move_unit(const BeltUnitSnapshot &unit, std::span<const BeltUnitSnapshot> units, float top, float bottom, double delta) {
    if (unit.dead || unit.manual || unit.speed <= 0.0F || bottom - top <= 2.0F * unit.config.edge_inset) {
        return {.id = unit.id, .next_y = unit.position.y};
    }
    State &state = states_[unit.id.value];
    float destination = unit.position.y;
    if (unit.approach_id.is_valid()) {
        const float assigned_y = slot_y(unit, state.slot, top, bottom);
        const float tolerance = unit.attack_mode == AttackMode::RANGED ? band_for(unit) : unit.config.arrival_dead_zone;
        if (std::abs(unit.position.y - assigned_y) > tolerance) {
            destination = assigned_y;
        }
    }
    float separation = 0.0F;
    for (const BeltUnitSnapshot &other : units) {
        separation += separation_from(unit, other);
    }
    const float yield = unit.attacking ? unit.config.attacking_yield : unit.config.moving_yield;
    destination += separation * unit.config.separation_weight * yield;
    destination = std::clamp(destination, top + unit.config.edge_inset, bottom - unit.config.edge_inset);
    const float difference = destination - unit.position.y;
    if (std::abs(difference) <= unit.config.arrival_dead_zone) {
        state.velocity = 0.0F;
        return {.id = unit.id, .next_y = unit.position.y};
    }
    const float max_speed = unit.speed * unit.attack_y_speed_scale;
    const float wanted_velocity = std::copysign(max_speed, difference);
    const float max_change = unit.config.acceleration * static_cast<float>(delta);
    state.velocity += std::clamp(wanted_velocity - state.velocity, -max_change, max_change);
    const float step = std::clamp(state.velocity * static_cast<float>(delta), -std::abs(difference), std::abs(difference));
    return {.id = unit.id, .next_y = std::clamp(unit.position.y + step, top + unit.config.edge_inset, bottom - unit.config.edge_inset)};
}

std::vector<BeltPositionResult> BeltPositioning::advance(std::span<const BeltUnitSnapshot> units, float top, float bottom, double delta) {
    std::vector<BeltUnitSnapshot> ordered(units.begin(), units.end());
    std::ranges::sort(ordered, [](const BeltUnitSnapshot &left, const BeltUnitSnapshot &right) { return left.id.value < right.id.value; });
    std::vector<BeltPositionResult> results;
    results.reserve(ordered.size());
    if (delta <= 0.0 || bottom <= top) {
        for (const BeltUnitSnapshot &unit : ordered) {
            results.push_back({.id = unit.id, .next_y = unit.position.y});
        }
        return results;
    }
    retain_assignments(ordered, top, bottom);
    for (const BeltUnitSnapshot &unit : ordered) {
        results.push_back(move_unit(unit, ordered, top, bottom, delta));
    }
    return results;
}

} // namespace defn
