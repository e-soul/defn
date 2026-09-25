// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "belt_positioning.h"

#include <algorithm>
#include <array>

namespace defn {

namespace {

BeltUnitSnapshot make_unit(uint64_t id, float y) {
    BeltUnitSnapshot unit;
    unit.id = {.value = id};
    unit.position = {.x = 100.0F, .y = y};
    unit.side = UnitSide::FRIENDLY;
    unit.speed = 100.0F;
    unit.config.acceleration = 1000.0F;
    unit.config.arrival_dead_zone = 0.0F;
    unit.config.edge_inset = 0.0F;
    return unit;
}

float find_y(const std::vector<BeltPositionResult> &results, uint64_t id) {
    const auto found = std::ranges::find_if(results, [id](const BeltPositionResult &result) { return result.id.value == id; });
    DEFN_REQUIRE(found != results.end());
    return found->next_y;
}

} // namespace

DEFN_TEST(belt_positioning_separates_coincident_units_deterministically) {
    const std::array<BeltUnitSnapshot, 2> units{make_unit(1, 800.0F), make_unit(2, 800.0F)};
    BeltPositioning forward;
    const auto first = forward.advance(units, 700.0F, 900.0F, 0.1);
    DEFN_CHECK(find_y(first, 1) < 800.0F);
    DEFN_CHECK(find_y(first, 2) > 800.0F);

    const std::array<BeltUnitSnapshot, 2> reversed{units[1], units[0]};
    BeltPositioning backward;
    const auto second = backward.advance(reversed, 700.0F, 900.0F, 0.1);
    DEFN_CHECK_CLOSE(find_y(first, 1), find_y(second, 1), 0.001);
    DEFN_CHECK_CLOSE(find_y(first, 2), find_y(second, 2), 0.001);
}

DEFN_TEST(belt_positioning_retains_survivor_slots_after_a_death) {
    BeltPositioning solver;
    std::array<BeltUnitSnapshot, 3> units{make_unit(1, 800.0F), make_unit(2, 830.0F), make_unit(3, 860.0F)};
    for (BeltUnitSnapshot &unit : units) {
        unit.config.melee_band = 90.0F;
        unit.approach_id = {.value = 10};
        unit.approach_position = {.x = 200.0F, .y = 800.0F};
        unit.position.x += static_cast<float>(unit.id.value) * 50.0F;
    }
    (void)solver.advance(units, 700.0F, 900.0F, 0.1);
    std::array<BeltUnitSnapshot, 3> survivors{units[1], units[2], make_unit(4, 800.0F)};
    survivors[2].approach_id = {.value = 10};
    survivors[2].config.melee_band = 90.0F;
    survivors[2].approach_position = {.x = 200.0F, .y = 800.0F};
    const auto result = solver.advance(survivors, 700.0F, 900.0F, 0.1);
    DEFN_CHECK_CLOSE(find_y(result, 3), 860.0F, 0.001);
    DEFN_CHECK_CLOSE(find_y(result, 4), 800.0F, 0.001);
}

DEFN_TEST(belt_positioning_manual_unit_stays_while_others_yield) {
    BeltPositioning solver;
    std::array<BeltUnitSnapshot, 2> units{make_unit(1, 800.0F), make_unit(2, 800.0F)};
    units[0].manual = true;
    const auto result = solver.advance(units, 700.0F, 900.0F, 0.1);
    DEFN_CHECK_CLOSE(find_y(result, 1), 800.0F, 0.001);
    DEFN_CHECK(find_y(result, 2) > 800.0F);
}

DEFN_TEST(belt_positioning_plants_during_an_attack_then_moves_when_idle) {
    BeltPositioning solver;
    BeltUnitSnapshot unit = make_unit(1, 860.0F);
    unit.approach_id = {.value = 10};
    unit.approach_position = {.x = 200.0F, .y = 800.0F};
    unit.attacking = true;
    for (int tick = 0; tick < 60; ++tick) {
        unit.position.y = find_y(solver.advance(std::span<const BeltUnitSnapshot>(&unit, 1), 700.0F, 900.0F, 1.0 / 60.0), 1);
    }
    DEFN_CHECK_CLOSE(unit.position.y, 860.0F, 0.001);
    unit.attacking = false;
    for (int tick = 0; tick < 60; ++tick) {
        unit.position.y = find_y(solver.advance(std::span<const BeltUnitSnapshot>(&unit, 1), 700.0F, 900.0F, 1.0 / 60.0), 1);
    }
    DEFN_CHECK(unit.position.y < 850.0F);
}

DEFN_TEST(belt_positioning_skips_tiny_adjustments_but_finishes_a_started_move) {
    BeltPositioning solver;
    BeltUnitSnapshot unit = make_unit(1, 800.0F);
    unit.approach_id = {.value = 10};
    unit.approach_position = {.x = 200.0F, .y = 812.0F};
    for (int tick = 0; tick < 60; ++tick) {
        unit.position.y = find_y(solver.advance(std::span<const BeltUnitSnapshot>(&unit, 1), 700.0F, 900.0F, 1.0 / 60.0), 1);
    }
    DEFN_CHECK_CLOSE(unit.position.y, 800.0F, 0.001);

    unit.approach_position.y = 830.0F;
    for (int tick = 0; tick < 120; ++tick) {
        unit.position.y = find_y(solver.advance(std::span<const BeltUnitSnapshot>(&unit, 1), 700.0F, 900.0F, 1.0 / 60.0), 1);
    }
    DEFN_CHECK(unit.position.y > 825.0F);
}

DEFN_TEST(belt_positioning_reassigns_on_target_change_and_respects_edges) {
    BeltPositioning solver;
    BeltUnitSnapshot unit = make_unit(1, 850.0F);
    unit.approach_id = {.value = 10};
    unit.approach_position = {.x = 200.0F, .y = 800.0F};
    (void)solver.advance(std::span<const BeltUnitSnapshot>(&unit, 1), 760.0F, 860.0F, 0.1);

    unit.approach_id = {.value = 20};
    unit.approach_position.y = 760.0F;
    for (int tick = 0; tick < 120; ++tick) {
        unit.position.y = find_y(solver.advance(std::span<const BeltUnitSnapshot>(&unit, 1), 760.0F, 860.0F, 1.0 / 60.0), 1);
        DEFN_CHECK(unit.position.y >= 760.0F);
        DEFN_CHECK(unit.position.y <= 860.0F);
    }
    DEFN_CHECK(unit.position.y < 850.0F);
}

DEFN_TEST(belt_positioning_plants_ranged_fire_and_repositions_between_shots_after_retargeting) {
    BeltPositioning solver;
    BeltUnitSnapshot unit = make_unit(1, 850.0F);
    unit.attack_mode = AttackMode::RANGED;
    unit.target_id = {.value = 10};
    unit.approach_id = unit.target_id;
    unit.approach_position = {.x = 200.0F, .y = 800.0F};
    unit.config.ranged_base_tolerance = 0.0F;
    unit.config.ranged_angle_slope = 0.0F;
    unit.config.ranged_max_tolerance = 0.0F;

    unit.attacking = true;
    DEFN_CHECK_CLOSE(find_y(solver.advance(std::span<const BeltUnitSnapshot>(&unit, 1), 700.0F, 900.0F, 0.1), 1), 850.0F, 0.001);
    unit.attacking = false;
    DEFN_CHECK_CLOSE(find_y(solver.advance(std::span<const BeltUnitSnapshot>(&unit, 1), 700.0F, 900.0F, 0.1), 1), 850.0F, 0.001);

    unit.target_id = {.value = 20};
    unit.approach_id = unit.target_id;
    unit.approach_position.y = 750.0F;
    unit.attacking = true;
    DEFN_CHECK_CLOSE(find_y(solver.advance(std::span<const BeltUnitSnapshot>(&unit, 1), 700.0F, 900.0F, 0.1), 1), 850.0F, 0.001);
    unit.attacking = false;
    DEFN_CHECK(find_y(solver.advance(std::span<const BeltUnitSnapshot>(&unit, 1), 700.0F, 900.0F, 0.1), 1) < 850.0F);
}

} // namespace defn
