#include "test_harness.h"

#include "combat_logic.h"
#include "combat_use_cases.h"
#include "projectile_rules.h"

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
    DEFN_CHECK_CLOSE(get_forward_distance(UnitSide::FRIENDLY, {.x = 10.0F, .y = 0.0F}, {.x = 25.0F, .y = 0.0F}), 15.0, 0.001);
    DEFN_CHECK_CLOSE(get_forward_distance(UnitSide::HOSTILE, {.x = 25.0F, .y = 0.0F}, {.x = 10.0F, .y = 0.0F}), 15.0, 0.001);
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
        {.id = ranged_target, .side = UnitSide::HOSTILE, .dead = false, .position = {.x = 90.0F, .y = 0.0F}},
        {.id = melee_target, .side = UnitSide::HOSTILE, .dead = false, .position = {.x = 25.0F, .y = 0.0F}},
    }};

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_combat_config(), {}, snapshots);
    DEFN_CHECK(selection.engaged);
    DEFN_CHECK_EQ(selection.attack_mode, AttackMode::MELEE);
    DEFN_CHECK_EQ(selection.target_id, melee_target);
    DEFN_CHECK_CLOSE(selection.target_position.x, 25.0, 0.001);
}

DEFN_TEST(select_target_keeps_current_target_when_still_in_range) {
    const EntityId current_target{.value = 7};
    const EntityId closer_target{.value = 8};

    const std::array<CombatTargetSnapshot, 2> snapshots{{
        {.id = current_target, .side = UnitSide::HOSTILE, .dead = false, .position = {.x = 90.0F, .y = 0.0F}},
        {.id = closer_target, .side = UnitSide::HOSTILE, .dead = false, .position = {.x = 20.0F, .y = 0.0F}},
    }};

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_combat_config(), current_target, snapshots);
    DEFN_CHECK(selection.engaged);
    DEFN_CHECK_EQ(selection.attack_mode, AttackMode::RANGED);
    DEFN_CHECK_EQ(selection.target_id, current_target);
    DEFN_CHECK_CLOSE(selection.target_position.x, 90.0, 0.001);
}

DEFN_TEST(select_target_skips_invalid_same_side_dead_and_behind_targets) {
    const EntityId invalid_target{};
    const EntityId same_side_target{.value = 9};
    const EntityId dead_target{.value = 10};
    const EntityId behind_target{.value = 11};

    const std::array<CombatTargetSnapshot, 4> snapshots{{
        {.id = invalid_target, .side = UnitSide::HOSTILE, .dead = false, .position = {.x = 15.0F, .y = 0.0F}},
        {.id = same_side_target, .side = UnitSide::FRIENDLY, .dead = false, .position = {.x = 20.0F, .y = 0.0F}},
        {.id = dead_target, .side = UnitSide::HOSTILE, .dead = true, .position = {.x = 25.0F, .y = 0.0F}},
        {.id = behind_target, .side = UnitSide::HOSTILE, .dead = false, .position = {.x = -25.0F, .y = 0.0F}},
    }};

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_combat_config(), {}, snapshots);
    DEFN_CHECK(!selection.engaged);
    DEFN_CHECK_EQ(selection.attack_mode, AttackMode::NONE);
    DEFN_CHECK(!selection.target_id.is_valid());
}

DEFN_TEST(select_target_uses_hostile_forward_direction) {
    CombatConfig config = make_combat_config();
    config.side = UnitSide::HOSTILE;

    const EntityId forward_target{.value = 12};
    const std::array<CombatTargetSnapshot, 1> snapshots{{
        {.id = forward_target, .side = UnitSide::FRIENDLY, .dead = false, .position = {.x = 60.0F, .y = 0.0F}},
    }};

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{.x = 100.0F, .y = 0.0F}, config, {}, snapshots);
    DEFN_CHECK(selection.engaged);
    DEFN_CHECK_EQ(selection.attack_mode, AttackMode::MELEE);
    DEFN_CHECK_EQ(selection.target_id, forward_target);
    DEFN_CHECK_CLOSE(selection.target_position.x, 60.0, 0.001);
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

DEFN_TEST(advance_combat_returns_damage_and_effect_commands_for_melee_attack) {
    const EntityId target{.value = 21};

    CombatConfig config = make_combat_config();
    config.melee_damage = 17;
    config.melee_flash_color = {.r = 0.8F, .g = 0.2F, .b = 0.1F, .a = 1.0F};

    CombatLogicInput input;
    input.selection = {.engaged = true, .attack_mode = AttackMode::MELEE, .target_id = target, .target_position = {.x = 20.0F, .y = 0.0F}};
    input.current_pose = CombatPoseState::WALK;

    const AdvanceCombatOutput output = advance_combat(config, input);
    DEFN_REQUIRE(output.commands.size() == 6);
    DEFN_CHECK_EQ(output.commands[0].type, CombatCommandType::STOP);
    DEFN_CHECK_EQ(output.commands[1].type, CombatCommandType::PLAY_POSE);
    DEFN_CHECK_EQ(output.commands[1].pose, CombatPoseIntent::ATTACK);
    DEFN_CHECK_EQ(output.commands[2].type, CombatCommandType::HIDE_MUZZLE_FLASH);
    DEFN_CHECK_EQ(output.commands[3].type, CombatCommandType::PLAY_EFFECT);
    DEFN_CHECK_EQ(output.commands[3].effect, CombatEffectType::MELEE_ATTACK);
    DEFN_CHECK_EQ(output.commands[4].type, CombatCommandType::DEAL_DAMAGE);
    DEFN_CHECK_EQ(output.commands[4].target_id, target);
    DEFN_CHECK_EQ(output.commands[4].damage, 17);
    DEFN_CHECK_EQ(output.commands[5].type, CombatCommandType::PLAY_EFFECT);
    DEFN_CHECK_EQ(output.commands[5].effect, CombatEffectType::DAMAGE_FLASH);
    DEFN_CHECK_CLOSE(output.commands[5].color.r, 0.8, 0.001);
}

DEFN_TEST(advance_combat_returns_projectile_spawn_command_for_projectile_attack) {
    const EntityId target{.value = 22};

    CombatConfig config = make_combat_config();
    config.projectile_attack = ProjectileDamageConfig{.splash_radius = 50.0F, .impact_damage = 44};

    CombatLogicInput input;
    input.selection = {.engaged = true, .attack_mode = AttackMode::RANGED, .target_id = target, .target_position = {.x = 90.0F, .y = 4.0F}};
    input.current_pose = CombatPoseState::WALK;

    const AdvanceCombatOutput output = advance_combat(config, input);
    DEFN_REQUIRE(output.commands.size() == 4);
    DEFN_CHECK_EQ(output.commands[0].type, CombatCommandType::STOP);
    DEFN_CHECK_EQ(output.commands[1].type, CombatCommandType::PLAY_POSE);
    DEFN_CHECK_EQ(output.commands[1].pose, CombatPoseIntent::SHOOT);
    DEFN_CHECK_EQ(output.commands[2].type, CombatCommandType::SPAWN_PROJECTILE);
    DEFN_CHECK_EQ(output.commands[2].target_id, target);
    DEFN_CHECK_CLOSE(output.commands[2].target_position.x, 90.0, 0.001);
    DEFN_CHECK_EQ(output.commands[2].projectile.impact_damage.value_or(0), 44);
    DEFN_CHECK_EQ(output.commands[3].type, CombatCommandType::PLAY_EFFECT);
    DEFN_CHECK_EQ(output.commands[3].effect, CombatEffectType::RANGED_SHOOT);
}

DEFN_TEST(resolve_projectile_impact_applies_direct_and_filtered_splash_damage) {
    const EntityId direct_target{.value = 31};
    const EntityId splash_target{.value = 32};
    const EntityId far_target{.value = 33};
    const EntityId friendly_target{.value = 34};

    ProjectileDamageConfig config;
    config.splash_radius = 60.0F;
    config.affected_fraction = 1.0F;
    config.min_affected_targets = 1;
    config.impact_damage = 40;
    config.splash_damage = 15;

    const std::array<ProjectileTargetSnapshot, 4> targets{{
        {.id = direct_target, .side = UnitSide::HOSTILE, .dead = false, .position = {.x = 100.0F, .y = 0.0F}},
        {.id = splash_target, .side = UnitSide::HOSTILE, .dead = false, .position = {.x = 130.0F, .y = 0.0F}},
        {.id = far_target, .side = UnitSide::HOSTILE, .dead = false, .position = {.x = 260.0F, .y = 0.0F}},
        {.id = friendly_target, .side = UnitSide::FRIENDLY, .dead = false, .position = {.x = 120.0F, .y = 0.0F}},
    }};

    const std::vector<ProjectileDamageCommand> commands = resolve_projectile_impact({
        .config = config,
        .shooter_side = UnitSide::FRIENDLY,
        .impact_position = {.x = 100.0F, .y = 0.0F},
        .direct_target_id = direct_target,
        .fallback_damage = 25,
        .targets = targets,
    });

    DEFN_REQUIRE(commands.size() == 2);
    DEFN_CHECK_EQ(commands[0].target_id, direct_target);
    DEFN_CHECK_EQ(commands[0].damage, 40);
    DEFN_CHECK_EQ(commands[1].target_id, splash_target);
    DEFN_CHECK_EQ(commands[1].damage, 15);
}

DEFN_TEST(compute_affected_projectile_target_count_uses_rounding_and_minimums) {
    ProjectileDamageConfig config;
    config.affected_fraction = 0.25F;
    config.min_affected_targets = 2;
    config.affected_target_rounding = SplashTargetRoundingMode::FLOOR;

    DEFN_CHECK_EQ(compute_affected_projectile_target_count(config, 5), 2);

    config.min_affected_targets = 1;
    config.affected_target_rounding = SplashTargetRoundingMode::CEIL;
    DEFN_CHECK_EQ(compute_affected_projectile_target_count(config, 5), 2);
}

} // namespace defn