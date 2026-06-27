#include "test_harness.h"

#include "combat_logic.h"

#include <array>

namespace defn {

namespace {

CombatConfig make_combat_config() {
    CombatConfig config;
    config.side = UnitSide::FRIENDLY;
    config.attack_range = 40.0F;
    config.ranged_range = 120.0F;
    config.melee_attack_period_seconds = 1.0;
    config.ranged_attack_period_seconds = 0.5;
    return config;
}

} // namespace

DEFN_TEST(forward_distance_respects_unit_side) {
    DEFN_CHECK_CLOSE(get_forward_distance(UnitSide::FRIENDLY, Vector2(10.0, 0.0), Vector2(25.0, 0.0)), 15.0, 0.001);
    DEFN_CHECK_CLOSE(get_forward_distance(UnitSide::HOSTILE, Vector2(25.0, 0.0), Vector2(10.0, 0.0)), 15.0, 0.001);
}

DEFN_TEST(classify_target_by_distance_respects_attack_ranges) {
    const CombatConfig config = make_combat_config();

    DEFN_CHECK_EQ(classify_target_by_distance(config, -1.0F), AttackMode::NONE);
    DEFN_CHECK_EQ(classify_target_by_distance(config, 40.0F), AttackMode::MELEE);
    DEFN_CHECK_EQ(classify_target_by_distance(config, 80.0F), AttackMode::RANGED);
    DEFN_CHECK_EQ(classify_target_by_distance(config, 121.0F), AttackMode::NONE);
}

DEFN_TEST(select_target_prefers_closest_melee_target) {
    const EntityId melee_target{.value = 1};
    const EntityId ranged_target{.value = 2};

    const std::array<CombatTargetSnapshot, 2> snapshots{{
        {.id = ranged_target, .side = UnitSide::HOSTILE, .dead = false, .position = Vector2(90.0, 0.0)},
        {.id = melee_target, .side = UnitSide::HOSTILE, .dead = false, .position = Vector2(25.0, 0.0)},
    }};

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2(0.0, 0.0), make_combat_config(), {}, snapshots);
    DEFN_CHECK(selection.engaged);
    DEFN_CHECK_EQ(selection.attack_mode, AttackMode::MELEE);
    DEFN_CHECK_EQ(selection.target_id, melee_target);
}

DEFN_TEST(select_target_keeps_current_target_when_still_in_range) {
    const EntityId current_target{.value = 7};
    const EntityId closer_target{.value = 8};

    const std::array<CombatTargetSnapshot, 2> snapshots{{
        {.id = current_target, .side = UnitSide::HOSTILE, .dead = false, .position = Vector2(90.0, 0.0)},
        {.id = closer_target, .side = UnitSide::HOSTILE, .dead = false, .position = Vector2(20.0, 0.0)},
    }};

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2(0.0, 0.0), make_combat_config(), current_target, snapshots);
    DEFN_CHECK(selection.engaged);
    DEFN_CHECK_EQ(selection.attack_mode, AttackMode::RANGED);
    DEFN_CHECK_EQ(selection.target_id, current_target);
}

DEFN_TEST(select_target_skips_invalid_same_side_dead_and_behind_targets) {
    const EntityId invalid_target{};
    const EntityId same_side_target{.value = 9};
    const EntityId dead_target{.value = 10};
    const EntityId behind_target{.value = 11};

    const std::array<CombatTargetSnapshot, 4> snapshots{{
        {.id = invalid_target, .side = UnitSide::HOSTILE, .dead = false, .position = Vector2(15.0, 0.0)},
        {.id = same_side_target, .side = UnitSide::FRIENDLY, .dead = false, .position = Vector2(20.0, 0.0)},
        {.id = dead_target, .side = UnitSide::HOSTILE, .dead = true, .position = Vector2(25.0, 0.0)},
        {.id = behind_target, .side = UnitSide::HOSTILE, .dead = false, .position = Vector2(-25.0, 0.0)},
    }};

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2(0.0, 0.0), make_combat_config(), {}, snapshots);
    DEFN_CHECK(!selection.engaged);
    DEFN_CHECK_EQ(selection.attack_mode, AttackMode::NONE);
    DEFN_CHECK(!selection.target_id.is_valid());
}

DEFN_TEST(select_target_uses_hostile_forward_direction) {
    CombatConfig config = make_combat_config();
    config.side = UnitSide::HOSTILE;

    const EntityId forward_target{.value = 12};
    const std::array<CombatTargetSnapshot, 1> snapshots{{
        {.id = forward_target, .side = UnitSide::FRIENDLY, .dead = false, .position = Vector2(60.0, 0.0)},
    }};

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2(100.0, 0.0), config, {}, snapshots);
    DEFN_CHECK(selection.engaged);
    DEFN_CHECK_EQ(selection.attack_mode, AttackMode::MELEE);
    DEFN_CHECK_EQ(selection.target_id, forward_target);
}

DEFN_TEST(advance_combat_logic_triggers_attack_when_cooldown_is_ready) {
    const EntityId target{.value = 3};

    CombatLogicInput input;
    input.selection = {.engaged = true, .attack_mode = AttackMode::RANGED, .target_id = target};
    input.current_pose = CombatPoseState::WALK;
    input.delta = 0.1;

    const CombatLogicStep step = advance_combat_logic(make_combat_config(), input);
    DEFN_CHECK_EQ(step.intent.movement, CombatMovementIntent::STOP);
    DEFN_CHECK_EQ(step.intent.pose, CombatPoseIntent::SHOOT);
    DEFN_CHECK(step.intent.trigger_attack);
    DEFN_CHECK_CLOSE(step.state.attack_cooldown_seconds, 0.5, 0.001);
}

DEFN_TEST(advance_combat_logic_does_not_attack_while_projectile_is_pending) {
    const EntityId target{.value = 13};

    CombatLogicInput input;
    input.selection = {.engaged = true, .attack_mode = AttackMode::RANGED, .target_id = target};
    input.delta = 0.1;
    input.projectile_pending = true;

    const CombatLogicStep step = advance_combat_logic(make_combat_config(), input);
    DEFN_CHECK_EQ(step.intent.movement, CombatMovementIntent::STOP);
    DEFN_CHECK(!step.intent.trigger_attack);
    DEFN_CHECK_EQ(step.state.attack_mode, AttackMode::RANGED);
    DEFN_CHECK_EQ(step.state.target_id, target);
}

DEFN_TEST(advance_combat_logic_respects_existing_cooldown) {
    const EntityId target{.value = 14};

    CombatLogicInput input;
    input.state = {.attack_cooldown_seconds = 0.4, .attack_mode = AttackMode::MELEE, .engaged = true, .target_id = target};
    input.selection = {.engaged = true, .attack_mode = AttackMode::MELEE, .target_id = target};
    input.current_pose = CombatPoseState::ATTACK;
    input.delta = 0.1;

    const CombatLogicStep step = advance_combat_logic(make_combat_config(), input);
    DEFN_CHECK_EQ(step.intent.movement, CombatMovementIntent::STOP);
    DEFN_CHECK_EQ(step.intent.pose, CombatPoseIntent::NONE);
    DEFN_CHECK(!step.intent.trigger_attack);
    DEFN_CHECK_CLOSE(step.state.attack_cooldown_seconds, 0.3, 0.001);
}

DEFN_TEST(advance_combat_logic_returns_current_state_when_unit_is_dead) {
    const EntityId target{.value = 15};

    CombatLogicInput input;
    input.state = {.attack_cooldown_seconds = 0.4, .attack_mode = AttackMode::RANGED, .engaged = true, .target_id = target};
    input.unit_dead = true;
    input.delta = 0.1;

    const CombatLogicStep step = advance_combat_logic(make_combat_config(), input);
    DEFN_CHECK_EQ(step.intent.movement, CombatMovementIntent::NONE);
    DEFN_CHECK(!step.intent.trigger_attack);
    DEFN_CHECK_EQ(step.state.attack_mode, AttackMode::RANGED);
    DEFN_CHECK_EQ(step.state.target_id, target);
    DEFN_CHECK_CLOSE(step.state.attack_cooldown_seconds, 0.4, 0.001);
}

DEFN_TEST(advance_combat_logic_resets_and_moves_when_disengaged) {
    const EntityId target{.value = 4};

    CombatLogicInput input;
    input.state = {.attack_cooldown_seconds = 0.3, .attack_mode = AttackMode::RANGED, .engaged = true, .target_id = target};
    input.current_pose = CombatPoseState::SHOOT;
    input.delta = 0.1;

    const CombatLogicStep step = advance_combat_logic(make_combat_config(), input);
    DEFN_CHECK_EQ(step.intent.movement, CombatMovementIntent::MOVE);
    DEFN_CHECK_EQ(step.intent.pose, CombatPoseIntent::WALK);
    DEFN_CHECK(step.intent.hide_muzzle_flash);
    DEFN_CHECK_EQ(step.state.attack_mode, AttackMode::NONE);
    DEFN_CHECK(!step.state.target_id.is_valid());
    DEFN_CHECK_CLOSE(step.state.attack_cooldown_seconds, 0.0, 0.001);
}

} // namespace defn