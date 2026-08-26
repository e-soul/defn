// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

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

// Three hostiles in a line, all inside ranged reach: near and frail, middling, far and tough. Every preference test
// below picks from exactly this field, so what changes between them is the rule and nothing else.
std::array<CombatTargetSnapshot, 3> make_preference_field() {
    return {
        CombatTargetSnapshot{.id = {.value = 1}, .side = UnitSide::HOSTILE, .position = {.x = 30.0F, .y = 0.0F}, .health = 20},
        CombatTargetSnapshot{.id = {.value = 2}, .side = UnitSide::HOSTILE, .position = {.x = 60.0F, .y = 0.0F}, .health = 50},
        CombatTargetSnapshot{.id = {.value = 3}, .side = UnitSide::HOSTILE, .position = {.x = 100.0F, .y = 0.0F}, .health = 90},
    };
}

// Ranged only: melee reach is 40, so the near candidate would otherwise be taken in contact and end the test early.
CombatConfig make_ranged_only_config(TargetPreference preference) {
    CombatConfig config = make_combat_config();
    config.attack_range = -1.0F;
    config.target_preference = preference;
    return config;
}

} // namespace

DEFN_TEST(target_preference_nearest_is_the_unchanged_rule) {
    const auto field = make_preference_field();

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_ranged_only_config(TargetPreference::NEAREST), {}, field);

    DEFN_CHECK(selection.engaged);
    DEFN_CHECK_EQ(selection.target_id.value, 1U);
}

// The marksman's job: reach past the front line to the backline that is shooting from behind it.
DEFN_TEST(target_preference_farthest_reaches_the_backline) {
    const auto field = make_preference_field();

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_ranged_only_config(TargetPreference::FARTHEST), {}, field);

    DEFN_CHECK_EQ(selection.target_id.value, 3U);
}

// The impact's job: answer the one hostile with real durability rather than spreading damage over the swarm.
DEFN_TEST(target_preference_highest_hp_answers_the_tough_target) {
    const auto field = make_preference_field();

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_ranged_only_config(TargetPreference::HIGHEST_HP), {}, field);

    DEFN_CHECK_EQ(selection.target_id.value, 3U);
}

DEFN_TEST(target_preference_lowest_hp_finishes_the_weakest) {
    const auto field = make_preference_field();

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_ranged_only_config(TargetPreference::LOWEST_HP), {}, field);

    DEFN_CHECK_EQ(selection.target_id.value, 1U);
}

// The breacher's job. Nothing about the shooter changes -- the pull is a property of the target, which is what makes
// it a tank role rather than a targeting quirk: unit A changes where damage lands on unit B.
DEFN_TEST(threat_weight_pulls_fire_off_the_nearest_target) {
    auto field = make_preference_field();
    field[1].threat_weight = 3.0F; // 60 away but three times as loud beats 30 away and quiet

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_ranged_only_config(TargetPreference::NEAREST), {}, field);

    DEFN_CHECK_EQ(selection.target_id.value, 2U);
}

// The line the plan drew: aggro changes who you shoot, never who you are standing next to.
DEFN_TEST(threat_weight_leaves_melee_on_pure_distance) {
    auto field = make_preference_field();
    field[1].threat_weight = 10.0F;
    CombatConfig config = make_combat_config();
    config.attack_range = 120.0F; // everything is in contact reach

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, config, {}, field);

    DEFN_CHECK_EQ(selection.attack_mode, AttackMode::MELEE);
    DEFN_CHECK_EQ(selection.target_id.value, 1U);
}

// A preference reorders what is reachable; it never extends reach. The far candidate sits outside ranged range, so
// `farthest` has to settle for the one it can actually hit.
DEFN_TEST(target_preference_never_widens_the_range_gate) {
    auto field = make_preference_field();
    field[2].position.x = 400.0F;

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_ranged_only_config(TargetPreference::FARTHEST), {}, field);

    DEFN_CHECK(selection.engaged);
    DEFN_CHECK_EQ(selection.target_id.value, 2U);
}

// Retention, which is what kept aggro weight from ever mattering: a unit that had locked on never asked again.
DEFN_TEST(ranged_fire_keeps_a_target_that_nothing_clearly_beats) {
    auto field = make_preference_field();
    const CombatConfig config = make_ranged_only_config(TargetPreference::NEAREST);

    // Already shooting the nearest candidate, which under `nearest` nothing else can beat. The unit must stay on it
    // rather than churn, which is the half of the old behaviour worth keeping.
    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, config, {.value = 1}, field);

    DEFN_CHECK_EQ(selection.target_id.value, 1U);
}

DEFN_TEST(ranged_fire_switches_to_a_clearly_better_target) {
    auto field = make_preference_field();
    const CombatConfig config = make_ranged_only_config(TargetPreference::NEAREST);

    // Currently on the far candidate at 100; the near one at 30 is 70% better and well clear of the 25% margin.
    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, config, {.value = 3}, field);

    DEFN_CHECK_EQ(selection.target_id.value, 1U);
}

// The point of the whole probe: a heavy target can now pull a shooter off the one it is already firing at, instead
// of only winning the argument at first acquisition.
DEFN_TEST(threat_weight_can_pull_a_shooter_off_its_current_target) {
    auto field = make_preference_field();
    const CombatConfig config = make_ranged_only_config(TargetPreference::NEAREST);

    const CombatTargetSelection without_aggro = select_target_from_snapshots(Vector2{}, config, {.value = 1}, field);
    DEFN_CHECK_EQ(without_aggro.target_id.value, 1U);

    field[1].threat_weight = 4.0F; // 60 away at weight 4 scores 15, against the current target's 30
    const CombatTargetSelection with_aggro = select_target_from_snapshots(Vector2{}, config, {.value = 1}, field);

    DEFN_CHECK_EQ(with_aggro.target_id.value, 2U);
}

// A marksman shooting the backline does not drop it for whatever just walked into its face -- that is the job, and
// the fight it is supposed to lose.
DEFN_TEST(farthest_does_not_drop_the_backline_for_a_closer_target) {
    auto field = make_preference_field();

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_ranged_only_config(TargetPreference::FARTHEST), {.value = 3}, field);

    DEFN_CHECK_EQ(selection.target_id.value, 3U);
}

// Contact is exempt: a unit in melee stays in melee whatever is shouting for attention further out.
DEFN_TEST(melee_contact_ignores_the_retarget_margin) {
    auto field = make_preference_field();
    field[2].threat_weight = 10.0F;
    CombatConfig config = make_combat_config();
    config.attack_range = 40.0F;
    config.ranged_range = 200.0F;

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, config, {.value = 1}, field);

    DEFN_CHECK_EQ(selection.attack_mode, AttackMode::MELEE);
    DEFN_CHECK_EQ(selection.target_id.value, 1U);
}

// Armour: the first breakpoint in the rules, and the reason a shot's worth stops being a property of the shooter.
DEFN_TEST(armour_subtracts_flatly_and_never_blocks_a_shot_entirely) {
    DEFN_CHECK_EQ(damage_after_armour(19, 0), 19);
    DEFN_CHECK_EQ(damage_after_armour(19, 6), 13);
    DEFN_CHECK_EQ(damage_after_armour(6, 6), 1); // never zero: nothing is immune
    DEFN_CHECK_EQ(damage_after_armour(3, 6), 1);
    DEFN_CHECK_EQ(damage_after_armour(0, 6), 0); // but no damage is still no damage
    DEFN_CHECK_EQ(damage_after_armour(-5, 6), 0);
}

// The whole point, as a comparison the old rules could not express: two shot profiles of near-identical DPS come
// apart entirely once the target has armour. Against bare flesh the swarm of pinpricks wins; against armour 6 the
// heavy shot does twelve times the work.
DEFN_TEST(armour_separates_many_small_shots_from_few_big_ones) {
    constexpr int OPERATOR_SHOT = 6;
    constexpr int MARKSMAN_SHOT = 19;
    constexpr int SHOTS = 10;

    const int operator_bare = SHOTS * damage_after_armour(OPERATOR_SHOT, 0);
    const int marksman_bare = SHOTS * damage_after_armour(MARKSMAN_SHOT, 0);
    const int operator_armoured = SHOTS * damage_after_armour(OPERATOR_SHOT, 6);
    const int marksman_armoured = SHOTS * damage_after_armour(MARKSMAN_SHOT, 6);

    // Ten of each shot: 60 against 190 bare, so the marksman shot is worth about three times as much.
    DEFN_CHECK_EQ(operator_bare, 60);
    DEFN_CHECK_EQ(marksman_bare, 190);
    // Behind armour 6 the gap widens to thirteen times. That change in ratio is the non-additivity.
    DEFN_CHECK_EQ(operator_armoured, 10);
    DEFN_CHECK_EQ(marksman_armoured, 130);
    DEFN_CHECK(marksman_armoured / operator_armoured > marksman_bare / operator_bare);
}

// Shot-count rounding, the breakpoint that was already in the rules and had never been priced. Time to kill is
// `ceil(hp / damage)` shots, not `hp / dps`, so the last shot into a nearly-dead target throws its remainder away.
// This is the mirror of armour: it penalises big shots and rewards many small ones.
//
// These pin the shipped catalog's relationships, so a hostile HP edit that silently moves a unit across its
// breakpoint fails here rather than in a measurement three weeks later.
int shots_to_kill(int health, int damage) { return (health + damage - 1) / damage; }

DEFN_TEST(grime_costs_the_marksman_a_sixth_shot) {
    constexpr int GRIME_HP = 96;

    // 96 sits one point above five marksman shots, so the sixth is spent to throw most of itself away.
    DEFN_CHECK_EQ(shots_to_kill(GRIME_HP, 19), 6);
    DEFN_CHECK_EQ((6 * 19) - GRIME_HP, 18);

    // The small-shot units divide it exactly and waste nothing, which is the whole asymmetry.
    DEFN_CHECK_EQ(shots_to_kill(GRIME_HP, 8) * 8, GRIME_HP);
    DEFN_CHECK_EQ(shots_to_kill(GRIME_HP, 6) * 6, GRIME_HP);
}

DEFN_TEST(jackal_is_exactly_four_marksman_shots) {
    constexpr int JACKAL_HP = 76;

    // The opposite pole: nothing wasted for the big shot, and a remainder for everyone else. A hostile pair drawn
    // from these two columns is the case a mixed friendly force is supposed to answer.
    DEFN_CHECK_EQ(shots_to_kill(JACKAL_HP, 19) * 19, JACKAL_HP);
    DEFN_CHECK(shots_to_kill(JACKAL_HP, 8) * 8 > JACKAL_HP);
    DEFN_CHECK(shots_to_kill(JACKAL_HP, 6) * 6 > JACKAL_HP);
}

// Minimum range: the first rule that charges a unit for an advantage rather than handing it one.
DEFN_TEST(minimum_range_opens_a_dead_zone_between_contact_and_the_shot) {
    CombatConfig config = make_combat_config();
    config.attack_range = 40.0F;
    config.ranged_range = 200.0F;
    config.minimum_ranged_range = 100.0F;

    DEFN_CHECK_EQ(classify_target_by_distance(config, 30.0F), AttackMode::MELEE); // in contact
    DEFN_CHECK_EQ(classify_target_by_distance(config, 70.0F), AttackMode::NONE);  // the dead zone
    DEFN_CHECK_EQ(classify_target_by_distance(config, 99.0F), AttackMode::NONE);
    DEFN_CHECK_EQ(classify_target_by_distance(config, 100.0F), AttackMode::RANGED); // the minimum is inclusive
    DEFN_CHECK_EQ(classify_target_by_distance(config, 200.0F), AttackMode::RANGED);
    DEFN_CHECK_EQ(classify_target_by_distance(config, 201.0F), AttackMode::NONE); // beyond reach
}

DEFN_TEST(minimum_range_of_zero_leaves_the_gate_exactly_as_it_was) {
    const CombatConfig config = make_combat_config(); // minimum_ranged_range defaults to 0

    DEFN_CHECK_EQ(classify_target_by_distance(config, 0.0F), AttackMode::MELEE);
    DEFN_CHECK_EQ(classify_target_by_distance(config, 41.0F), AttackMode::RANGED);
    DEFN_CHECK_EQ(classify_target_by_distance(config, 120.0F), AttackMode::RANGED);
}

// A candidate inside the dead zone is not a ranged target, so the shooter reaches past it to one it can actually hit.
DEFN_TEST(minimum_range_makes_a_close_enemy_unshootable) {
    auto field = make_preference_field();
    CombatConfig config = make_ranged_only_config(TargetPreference::NEAREST);
    config.minimum_ranged_range = 50.0F; // the candidate at 30 is now inside the dead zone

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, config, {}, field);

    DEFN_CHECK(selection.engaged);
    DEFN_CHECK_EQ(selection.target_id.value, 2U);
}

// The weakness, stated as the fight it loses: everything has closed inside the dead zone and there is nothing to
// shoot at all.
DEFN_TEST(minimum_range_leaves_a_shooter_with_nothing_when_everything_closes) {
    auto field = make_preference_field();
    CombatConfig config = make_ranged_only_config(TargetPreference::NEAREST);
    config.minimum_ranged_range = 200.0F; // every candidate sits at 100 or nearer

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, config, {}, field);

    DEFN_CHECK(!selection.engaged);
}

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

DEFN_TEST(select_target_keeps_current_target_when_nothing_clearly_beats_it) {
    const EntityId current_target{.value = 7};
    const EntityId slightly_closer_target{.value = 8};

    const std::array<CombatTargetSnapshot, 2> snapshots{{
        {.id = current_target, .side = UnitSide::HOSTILE, .dead = false, .position = {.x = 90.0F, .y = 0.0F}},
        {.id = slightly_closer_target, .side = UnitSide::HOSTILE, .dead = false, .position = {.x = 80.0F, .y = 0.0F}},
    }};

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_combat_config(), current_target, snapshots);
    DEFN_CHECK(selection.engaged);
    DEFN_CHECK_EQ(selection.attack_mode, AttackMode::RANGED);
    DEFN_CHECK_EQ(selection.target_id, current_target);
    DEFN_CHECK_CLOSE(selection.target_position.x, 90.0, 0.001);
}

// The contract this replaced kept the distant target no matter what closed on the unit. Retention is now a margin
// rather than a lock, so something that walks into contact takes the shot -- and, more to the point, so can a target
// that is merely loud enough.
DEFN_TEST(select_target_gives_up_a_distant_target_for_one_in_contact) {
    const EntityId current_target{.value = 7};
    const EntityId closer_target{.value = 8};

    const std::array<CombatTargetSnapshot, 2> snapshots{{
        {.id = current_target, .side = UnitSide::HOSTILE, .dead = false, .position = {.x = 90.0F, .y = 0.0F}},
        {.id = closer_target, .side = UnitSide::HOSTILE, .dead = false, .position = {.x = 20.0F, .y = 0.0F}},
    }};

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_combat_config(), current_target, snapshots);
    DEFN_CHECK(selection.engaged);
    DEFN_CHECK_EQ(selection.attack_mode, AttackMode::MELEE);
    DEFN_CHECK_EQ(selection.target_id, closer_target);
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

DEFN_TEST(advance_combat_logic_suspends_all_automatic_intents_while_repositioning) {
    const EntityId target{.value = 16};

    CombatLogicInput input;
    input.state = {.attack_cooldown_seconds = 0.4, .attack_mode = AttackMode::RANGED, .engaged = true, .target_id = target};
    input.selection = {.engaged = true, .attack_mode = AttackMode::RANGED, .target_id = target};
    input.current_pose = CombatPoseState::SHOOT;
    input.delta = 0.1;
    input.projectile_pending = true;
    input.manual_repositioning = true;

    const CombatLogicStep step = advance_combat_logic(make_combat_config(), input);
    DEFN_CHECK_EQ(step.intent.movement, CombatMovementIntent::NONE);
    DEFN_CHECK_EQ(step.intent.pose, CombatPoseIntent::NONE);
    DEFN_CHECK(!step.intent.trigger_attack);
    DEFN_CHECK_EQ(step.state.attack_mode, AttackMode::NONE);
    DEFN_CHECK(!step.state.engaged);
    DEFN_CHECK(!step.state.target_id.is_valid());
    DEFN_CHECK_CLOSE(step.state.attack_cooldown_seconds, 0.3, 0.001);
}

DEFN_TEST(advance_combat_emits_no_commands_while_manual_repositioning) {
    CombatLogicInput input;
    input.state.attack_cooldown_seconds = 0.25;
    input.selection = {.engaged = true, .attack_mode = AttackMode::MELEE, .target_id = {.value = 17}};
    input.delta = 0.1;
    input.manual_repositioning = true;

    const AdvanceCombatOutput output = advance_combat(make_combat_config(), input);
    DEFN_CHECK(output.commands.empty());
    DEFN_CHECK_CLOSE(output.state.attack_cooldown_seconds, 0.15, 0.001);
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
    input.state = {.attack_cooldown_seconds = 0.4, .attack_mode = AttackMode::RANGED, .engaged = true, .target_id = target};
    input.current_pose = CombatPoseState::SHOOT;
    input.delta = 0.1;

    const CombatLogicStep step = advance_combat_logic(make_combat_config(), input);
    DEFN_CHECK_EQ(step.intent.movement, CombatMovementIntent::MOVE);
    DEFN_CHECK_EQ(step.intent.pose, CombatPoseIntent::WALK);
    DEFN_CHECK(step.intent.hide_muzzle_flash);
    DEFN_CHECK_EQ(step.state.attack_mode, AttackMode::NONE);
    DEFN_CHECK(!step.state.target_id.is_valid());
    DEFN_CHECK_CLOSE(step.state.attack_cooldown_seconds, 0.3, 0.001);
}

DEFN_TEST(advance_combat_logic_plays_backswing_out_when_the_target_died) {
    const EntityId target{.value = 4};

    CombatLogicInput input;
    input.state = {.attack_cooldown_seconds = 0.5, .attack_mode = AttackMode::RANGED, .engaged = true, .target_id = target};
    input.current_pose = CombatPoseState::SHOOT;
    input.delta = 0.1;
    input.attack_animation_playing = true;
    input.target_out_of_range = false;

    const CombatLogicStep step = advance_combat_logic(make_combat_config(), input);
    DEFN_CHECK_EQ(step.intent.movement, CombatMovementIntent::STOP);
    DEFN_CHECK_EQ(step.intent.pose, CombatPoseIntent::NONE);
    DEFN_CHECK(!step.intent.hide_muzzle_flash);
    DEFN_CHECK_EQ(step.state.attack_mode, AttackMode::NONE);
    DEFN_CHECK(!step.state.target_id.is_valid());
    DEFN_CHECK_CLOSE(step.state.attack_cooldown_seconds, 0.4, 0.001);
}

DEFN_TEST(advance_combat_logic_walks_once_the_backswing_animation_ends) {
    const EntityId target{.value = 4};

    CombatLogicInput input;
    input.state = {.attack_cooldown_seconds = 0.5, .attack_mode = AttackMode::RANGED, .engaged = true, .target_id = target};
    input.current_pose = CombatPoseState::SHOOT;
    input.delta = 0.1;
    input.attack_animation_playing = false;

    const CombatLogicStep step = advance_combat_logic(make_combat_config(), input);
    DEFN_CHECK_EQ(step.intent.movement, CombatMovementIntent::MOVE);
    DEFN_CHECK_EQ(step.intent.pose, CombatPoseIntent::WALK);
    DEFN_CHECK(step.state.attack_cooldown_seconds > 0.0);
}

DEFN_TEST(advance_combat_logic_holds_the_windup_even_when_the_target_fled) {
    const EntityId target{.value = 4};

    CombatLogicInput input;
    input.state = {.attack_cooldown_seconds = 0.5, .attack_mode = AttackMode::RANGED, .engaged = true, .target_id = target};
    input.current_pose = CombatPoseState::SHOOT;
    input.delta = 0.1;
    input.attack_animation_playing = true;
    input.attack_windup_active = true;
    input.target_out_of_range = true;

    const CombatLogicStep step = advance_combat_logic(make_combat_config(), input);
    DEFN_CHECK_EQ(step.intent.movement, CombatMovementIntent::STOP);
    DEFN_CHECK_EQ(step.intent.pose, CombatPoseIntent::NONE);
    DEFN_CHECK(!step.intent.hide_muzzle_flash);
}

DEFN_TEST(advance_combat_logic_cancels_the_backswing_to_chase_a_fled_target) {
    const EntityId target{.value = 4};

    CombatLogicInput input;
    input.state = {.attack_cooldown_seconds = 0.5, .attack_mode = AttackMode::RANGED, .engaged = true, .target_id = target};
    input.current_pose = CombatPoseState::SHOOT;
    input.delta = 0.1;
    input.attack_animation_playing = true;
    input.attack_windup_active = false;
    input.target_out_of_range = true;

    const CombatLogicStep step = advance_combat_logic(make_combat_config(), input);
    DEFN_CHECK_EQ(step.intent.movement, CombatMovementIntent::MOVE);
    DEFN_CHECK_EQ(step.intent.pose, CombatPoseIntent::WALK);
    DEFN_CHECK(step.intent.hide_muzzle_flash);
    DEFN_CHECK(step.state.attack_cooldown_seconds > 0.0);
}

DEFN_TEST(advance_combat_logic_keeps_cooldown_across_disengage_and_reacquire) {
    const EntityId first_target{.value = 4};
    const EntityId second_target{.value = 5};
    const CombatConfig config = make_combat_config();

    CombatLogicInput disengaged;
    disengaged.state = {.attack_cooldown_seconds = 0.5, .attack_mode = AttackMode::RANGED, .engaged = true, .target_id = first_target};
    disengaged.current_pose = CombatPoseState::SHOOT;
    disengaged.delta = 0.1;

    const CombatLogicStep after_disengage = advance_combat_logic(config, disengaged);
    DEFN_CHECK_CLOSE(after_disengage.state.attack_cooldown_seconds, 0.4, 0.001);

    CombatLogicInput reacquired;
    reacquired.state = after_disengage.state;
    reacquired.selection = {.engaged = true, .attack_mode = AttackMode::RANGED, .target_id = second_target, .target_position = {.x = 80.0F, .y = 0.0F}};
    reacquired.current_pose = CombatPoseState::SHOOT;
    reacquired.delta = 0.1;

    const CombatLogicStep after_reacquire = advance_combat_logic(config, reacquired);
    DEFN_CHECK(!after_reacquire.intent.trigger_attack);
    DEFN_CHECK_CLOSE(after_reacquire.state.attack_cooldown_seconds, 0.3, 0.001);
}

DEFN_TEST(advance_combat_logic_does_not_repose_on_mode_change_while_attack_animation_plays) {
    const EntityId target{.value = 6};

    CombatLogicInput input;
    input.state = {.attack_cooldown_seconds = 0.4, .attack_mode = AttackMode::RANGED, .engaged = true, .target_id = target};
    input.selection = {.engaged = true, .attack_mode = AttackMode::MELEE, .target_id = target, .target_position = {.x = 20.0F, .y = 0.0F}};
    input.current_pose = CombatPoseState::SHOOT;
    input.delta = 0.1;
    input.attack_animation_playing = true;

    const CombatLogicStep step = advance_combat_logic(make_combat_config(), input);
    DEFN_CHECK_EQ(step.intent.movement, CombatMovementIntent::STOP);
    DEFN_CHECK_EQ(step.intent.pose, CombatPoseIntent::NONE);
    DEFN_CHECK(!step.intent.hide_muzzle_flash);
    DEFN_CHECK(!step.intent.trigger_attack);
    DEFN_CHECK_EQ(step.state.attack_mode, AttackMode::MELEE);
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
