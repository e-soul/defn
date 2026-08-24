// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "projectile_flight.h"

namespace defn {

DEFN_TEST(projectile_flight_starts_at_the_muzzle_and_points_at_the_target) {
    const ProjectileFlight flight = begin_projectile_flight({.x = 100.0F, .y = 200.0F}, {.x = 400.0F, .y = 600.0F}, 500.0F);

    DEFN_CHECK_CLOSE(flight.position.x, 100.0, 0.001);
    DEFN_CHECK_CLOSE(flight.position.y, 200.0, 0.001);
    DEFN_CHECK_CLOSE(flight.total_distance, 500.0, 0.001); // a 300/400/500 triangle
    DEFN_CHECK_CLOSE(flight.direction.x, 0.6, 0.001);
    DEFN_CHECK_CLOSE(flight.direction.y, 0.8, 0.001);
    DEFN_CHECK_CLOSE(flight.travelled_distance, 0.0, 0.001);
    DEFN_CHECK(!projectile_arrives_immediately(flight));
}

DEFN_TEST(projectile_flight_advances_by_speed_times_delta) {
    ProjectileFlight flight = begin_projectile_flight({.x = 0.0F, .y = 0.0F}, {.x = 1000.0F, .y = 0.0F}, 500.0F);

    DEFN_CHECK(!advance_projectile(flight, 0.1).arrived);
    DEFN_CHECK_CLOSE(flight.position.x, 50.0, 0.001);
    DEFN_CHECK_CLOSE(flight.travelled_distance, 50.0, 0.001);

    DEFN_CHECK(!advance_projectile(flight, 0.5).arrived);
    DEFN_CHECK_CLOSE(flight.position.x, 300.0, 0.001);
}

DEFN_TEST(projectile_flight_snaps_to_the_target_on_arrival) {
    ProjectileFlight flight = begin_projectile_flight({.x = 0.0F, .y = 0.0F}, {.x = 100.0F, .y = 0.0F}, 500.0F);

    // A step that would overshoot lands exactly on the frozen target position instead.
    DEFN_CHECK(advance_projectile(flight, 1.0).arrived);
    DEFN_CHECK_CLOSE(flight.position.x, 100.0, 0.001);
    DEFN_CHECK_CLOSE(flight.travelled_distance, flight.total_distance, 0.001);
}

DEFN_TEST(projectile_flight_arrives_immediately_without_distance_or_speed) {
    const ProjectileFlight standing_still = begin_projectile_flight({.x = 50.0F, .y = 50.0F}, {.x = 50.0F, .y = 50.0F}, 500.0F);
    DEFN_CHECK(projectile_arrives_immediately(standing_still));

    const ProjectileFlight without_speed = begin_projectile_flight({.x = 0.0F, .y = 0.0F}, {.x = 100.0F, .y = 0.0F}, 0.0F);
    DEFN_CHECK(projectile_arrives_immediately(without_speed));
    DEFN_CHECK_CLOSE(without_speed.direction.x, 1.0, 0.001);
}

DEFN_TEST(projectile_flight_without_speed_detonates_on_the_next_step) {
    ProjectileFlight flight = begin_projectile_flight({.x = 0.0F, .y = 0.0F}, {.x = 100.0F, .y = 0.0F}, 0.0F);

    DEFN_CHECK(advance_projectile(flight, 0.1).arrived);
    DEFN_CHECK_CLOSE(flight.position.x, 100.0, 0.001);
}

DEFN_TEST(projectile_flight_ignores_a_zero_delta_by_arriving) {
    // The shipped node explodes rather than hanging when a frame advances it by nothing at all.
    ProjectileFlight flight = begin_projectile_flight({.x = 0.0F, .y = 0.0F}, {.x = 100.0F, .y = 0.0F}, 500.0F);

    DEFN_CHECK(advance_projectile(flight, 0.0).arrived);
    DEFN_CHECK_CLOSE(flight.position.x, 100.0, 0.001);
}

DEFN_TEST(projectile_flight_travels_a_diagonal_at_the_configured_speed) {
    ProjectileFlight flight = begin_projectile_flight({.x = 0.0F, .y = 0.0F}, {.x = 300.0F, .y = 400.0F}, 500.0F);

    DEFN_CHECK(!advance_projectile(flight, 0.5).arrived);
    DEFN_CHECK_CLOSE(flight.position.x, 150.0, 0.001);
    DEFN_CHECK_CLOSE(flight.position.y, 200.0, 0.001);

    DEFN_CHECK(advance_projectile(flight, 0.5).arrived);
    DEFN_CHECK_CLOSE(flight.position.x, 300.0, 0.001);
    DEFN_CHECK_CLOSE(flight.position.y, 400.0, 0.001);
}

} // namespace defn
