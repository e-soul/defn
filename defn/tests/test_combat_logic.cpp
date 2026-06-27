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