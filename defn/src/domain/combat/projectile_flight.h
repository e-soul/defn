// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef PROJECTILE_FLIGHT_H
#define PROJECTILE_FLIGHT_H

#include "content_values.h"

#include <cmath>

namespace defn {

// A shot in the air. It travels in a straight line at a fixed speed toward a position captured when it launched: there
// is no homing, so a target that walks away is missed. Detonation is a separate concern -- this only says where the
// shot is and whether it has arrived.
struct ProjectileFlight {
    Vector2 position;
    Vector2 target_position;
    Vector2 direction;
    float speed_pixels_per_second = 0.0F;
    float total_distance = 0.0F;
    float travelled_distance = 0.0F;
};

struct ProjectileFlightStep {
    bool arrived = false;
};

[[nodiscard]] inline ProjectileFlight begin_projectile_flight(const Vector2 &start, const Vector2 &target, float speed_pixels_per_second) {
    const Vector2 travel{.x = target.x - start.x, .y = target.y - start.y};
    const float total_distance = std::sqrt((travel.x * travel.x) + (travel.y * travel.y));

    return {
        .position = start,
        .target_position = target,
        .direction = total_distance > 0.0F ? Vector2{.x = travel.x / total_distance, .y = travel.y / total_distance} : Vector2{},
        .speed_pixels_per_second = speed_pixels_per_second,
        .total_distance = total_distance,
        .travelled_distance = 0.0F,
    };
}

// A shot with nowhere to go, or no speed to get there with, detonates the moment it is fired.
[[nodiscard]] inline bool projectile_arrives_immediately(const ProjectileFlight &flight) {
    return flight.total_distance <= 0.0F || flight.speed_pixels_per_second <= 0.0F;
}

inline ProjectileFlightStep advance_projectile(ProjectileFlight &flight, double delta) {
    const auto step_distance = static_cast<float>(static_cast<double>(flight.speed_pixels_per_second) * delta);
    const float remaining_distance = flight.total_distance - flight.travelled_distance;

    if (step_distance <= 0.0F || step_distance >= remaining_distance) {
        flight.position = flight.target_position;
        flight.travelled_distance = flight.total_distance;
        return {.arrived = true};
    }

    flight.travelled_distance += step_distance;
    flight.position.x += flight.direction.x * step_distance;
    flight.position.y += flight.direction.y * step_distance;
    return {};
}

} // namespace defn

#endif
