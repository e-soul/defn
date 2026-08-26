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

// The base carries `threat_weight: 0.25` in the catalog, and the number is doing a job no other unit's weight does:
// a weight below one is the only way to make a target *less* attractive than plain geometry says it is. Without it the
// tower is a normal candidate, and against `farthest` it is the winning one by construction -- nothing on the field is
// ever further away than the thing the hostiles are walking towards. A jackal that pushed the line to within its 620
// reach would then camp the tower and ignore the defence entirely.
//
// One weight covers all four preferences because every one of them consumes it, two by dividing and two by
// multiplying. This pins that: the same base loses to the same friendly under each rule.
DEFN_TEST(a_light_target_loses_to_a_friendlier_candidate_under_every_preference) {
    constexpr float BASE_THREAT_WEIGHT = 0.25F;

    // The tower, far off and lightly weighted, against one defender that is nearer, frailer and closer to full.
    const std::array<CombatTargetSnapshot, 2> field = {
        CombatTargetSnapshot{
            .id = {.value = 1}, .side = UnitSide::HOSTILE, .position = {.x = 100.0F, .y = 0.0F}, .threat_weight = BASE_THREAT_WEIGHT, .health = 300},
        CombatTargetSnapshot{.id = {.value = 2}, .side = UnitSide::HOSTILE, .position = {.x = 60.0F, .y = 0.0F}, .threat_weight = 1.0F, .health = 180},
    };

    for (const TargetPreference preference :
         {TargetPreference::NEAREST, TargetPreference::FARTHEST, TargetPreference::LOWEST_HP, TargetPreference::HIGHEST_HP}) {
        const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_ranged_only_config(preference), {}, field);
        DEFN_CHECK_EQ(selection.target_id.value, 2U);
    }

    // And at weight 1 the tower wins two of the four outright, which is what the catalog value is buying back.
    std::array<CombatTargetSnapshot, 2> unweighted = field;
    unweighted[0].threat_weight = 1.0F;

    DEFN_CHECK_EQ(select_target_from_snapshots(Vector2{}, make_ranged_only_config(TargetPreference::FARTHEST), {}, unweighted).target_id.value, 1U);
    DEFN_CHECK_EQ(select_target_from_snapshots(Vector2{}, make_ranged_only_config(TargetPreference::HIGHEST_HP), {}, unweighted).target_id.value, 1U);
    DEFN_CHECK_EQ(select_target_from_snapshots(Vector2{}, make_ranged_only_config(TargetPreference::LOWEST_HP), {}, unweighted).target_id.value, 2U);
}

// A marksman shooting the backline does not drop it for whatever just walked into its face -- that is the job, and
// the fight it is supposed to lose.
DEFN_TEST(farthest_does_not_drop_the_backline_for_a_closer_target) {
    auto field = make_preference_field();

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_ranged_only_config(TargetPreference::FARTHEST), {.value = 3}, field);

    DEFN_CHECK_EQ(selection.target_id.value, 3U);
}

// Pursuit. A shooter that prefers a role it can see but cannot yet reach declines the lesser target in front of it and
// keeps walking. There is no steering anywhere in this: combat and movement are both forward-only, so "advance on the
// target I want" is just "refuse to stop", and the existing disengaged path in advance_combat_logic does the rest.
namespace {

// Only the near candidate is shootable; the far one sits past the gun but inside the sensor. That gap is the mechanism.
CombatConfig make_pursuit_config(float sniper_bias) {
    CombatConfig config = make_ranged_only_config(TargetPreference::NEAREST);
    config.ranged_range = 50.0F;
    config.aggro_range = 150.0F;
    config.role_bias.fill(1.0F);
    config.role_bias.at(static_cast<std::size_t>(unit_role_index(UnitRole::SNIPER))) = sniper_bias;
    return config;
}

std::array<CombatTargetSnapshot, 3> make_role_field(UnitRole near_role, UnitRole far_role) {
    auto field = make_preference_field();
    field[0].role = near_role; // x = 30, inside the 50 gun
    field[2].role = far_role;  // x = 100, outside the gun and inside the 150 sensor
    return field;
}

} // namespace

DEFN_TEST(a_preferred_role_out_of_reach_suppresses_a_lesser_target_in_reach) {
    const auto field = make_role_field(UnitRole::ASSAULT, UnitRole::SNIPER);

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_pursuit_config(3.0F), {}, field);

    DEFN_CHECK(!selection.engaged);
    DEFN_CHECK(selection.pursuing);
    DEFN_CHECK(!selection.target_id.is_valid());
}

// The same field with the preference switched off has to engage, or the test above is measuring the range gate.
DEFN_TEST(without_a_role_preference_the_lesser_target_is_taken_as_before) {
    const auto field = make_role_field(UnitRole::ASSAULT, UnitRole::SNIPER);

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_pursuit_config(1.0F), {}, field);

    DEFN_CHECK(selection.engaged);
    DEFN_CHECK(!selection.pursuing);
    DEFN_CHECK_EQ(selection.target_id.value, 1U);
}

// Nothing to walk towards once the preferred thing is already shootable, so the shooter stops and fires.
DEFN_TEST(a_preferred_role_already_in_reach_ends_the_pursuit) {
    const auto field = make_role_field(UnitRole::SNIPER, UnitRole::SNIPER);

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, make_pursuit_config(3.0F), {}, field);

    DEFN_CHECK(selection.engaged);
    DEFN_CHECK_EQ(selection.target_id.value, 1U);
}

// Sensed means sensed: past the aggro range the preferred target is not a reason to do anything, or a unit would walk
// the length of the belt on the strength of something it cannot see.
DEFN_TEST(a_preferred_role_beyond_the_aggro_range_does_not_suppress) {
    const auto field = make_role_field(UnitRole::ASSAULT, UnitRole::SNIPER);
    CombatConfig config = make_pursuit_config(3.0F);
    config.aggro_range = 80.0F; // the far candidate is at 100

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, config, {}, field);

    DEFN_CHECK(selection.engaged);
    DEFN_CHECK_EQ(selection.target_id.value, 1U);
}

// A rusher walking through the front line is the whole point, so a melee target it has not yet committed to is
// suppressed exactly like a ranged one.
DEFN_TEST(pursuit_walks_a_rusher_past_a_contact_it_has_not_committed_to) {
    const auto field = make_role_field(UnitRole::TANK, UnitRole::SNIPER);
    CombatConfig config = make_pursuit_config(3.0F);
    config.attack_range = 40.0F; // the near candidate at 30 is in contact reach
    config.ranged_range = -1.0F; // melee only, which is what a rusher is

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, config, {}, field);

    DEFN_CHECK(!selection.engaged);
    DEFN_CHECK(selection.pursuing);
}

// But a fight already joined is not abandoned. Contact stickiness is checked before pursuit for the same reason it is
// checked before the retarget margin: walking a unit out of a swing is a movement change, not a targeting one.
DEFN_TEST(pursuit_never_pulls_a_unit_out_of_a_contact_it_is_already_in) {
    const auto field = make_role_field(UnitRole::TANK, UnitRole::SNIPER);
    CombatConfig config = make_pursuit_config(3.0F);
    config.attack_range = 40.0F;
    config.ranged_range = -1.0F;

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, config, {.value = 1}, field);

    DEFN_CHECK(selection.engaged);
    DEFN_CHECK_EQ(selection.attack_mode, AttackMode::MELEE);
    DEFN_CHECK_EQ(selection.target_id.value, 1U);
}

// The hound's catalog numbers, pinned as a pair, because neither is meaningful without the other. `aggro_range: 600`
// against a `melee_attack_range` of 128 is the whole dive: it has to see a sniper from well outside contact to have
// anything to decline. And both are read against the *friendly line's* spacing -- at the engagement lab's 70px default
// the back rank is already inside 128px when the front rank is, so there is no backline to reach and the mechanism
// measures as a flat null. See 2.11.
DEFN_TEST(the_hound_senses_a_sniper_far_outside_the_reach_it_kills_with) {
    constexpr float HOUND_AGGRO_RANGE = 600.0F;
    constexpr float HOUND_MELEE_RANGE = 128.0F;
    constexpr float LAB_FRIENDLY_SPACING = 70.0F;

    // The dive only exists in the gap between the two.
    DEFN_CHECK(HOUND_AGGRO_RANGE > HOUND_MELEE_RANGE * 4.0F);

    // And that gap is only reachable when the line it walks into is looser than its own reach.
    DEFN_CHECK(LAB_FRIENDLY_SPACING < HOUND_MELEE_RANGE); // the default lab cannot show a dive
    DEFN_CHECK(200.0F > HOUND_MELEE_RANGE);               // the spacing the measurement in 2.11 had to use
}

// The sensor can never be tighter than the gun, so an unset aggro range means "no pursuit" rather than "blind".
DEFN_TEST(aggro_range_is_floored_at_the_ranged_range) {
    CombatConfig config = make_combat_config();
    config.ranged_range = 300.0F;

    DEFN_CHECK_EQ(resolve_aggro_range(config), 300.0F); // unset
    config.aggro_range = 120.0F;
    DEFN_CHECK_EQ(resolve_aggro_range(config), 300.0F); // narrower than the gun, and ignored
    config.aggro_range = 700.0F;
    DEFN_CHECK_EQ(resolve_aggro_range(config), 700.0F);
}

// Role and threat weight multiply rather than replace, so a tank still drags a role-preferring shooter -- just not as
// far. 60 away at weight 3 scores 20 against the preferred sniper's 100 away at bias 2, which is 50.
DEFN_TEST(a_role_preference_and_a_threat_weight_compose) {
    auto field = make_preference_field();
    field[1].threat_weight = 3.0F;
    field[2].role = UnitRole::SNIPER;

    CombatConfig config = make_ranged_only_config(TargetPreference::NEAREST);
    config.role_bias.fill(1.0F);
    config.role_bias.at(static_cast<std::size_t>(unit_role_index(UnitRole::SNIPER))) = 2.0F;

    const CombatTargetSelection selection = select_target_from_snapshots(Vector2{}, config, {}, field);

    DEFN_CHECK_EQ(selection.target_id.value, 2U);
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

// Impact's contact profile, pinned, because the numbers only make sense against a *hound's transit time* and nothing
// in the catalog says so. Melee was byte-identical 15/1.0s across the whole roster before this; impact is the first
// unit whose job is the fight everyone else has no opinion about.
//
// The mechanism is a toll, not a wall. A diving hound declines impact -- it prefers snipers -- so impact never gets a
// stand-up fight. What it gets is the seconds the hound spends crossing its 128px reach, and the damage in that window
// is what decides whether the marksman behind it survives. Two impacts in the line is the threshold, which is what
// makes the value non-additive rather than a flat buff.
DEFN_TEST(impact_taxes_a_diver_for_roughly_half_its_health_on_the_way_past) {
    constexpr float IMPACT_MELEE_REACH = 128.0F;
    constexpr int IMPACT_MELEE_DAMAGE = 30;
    constexpr double IMPACT_MELEE_PERIOD = 1.0;
    constexpr float HOUND_SPEED = 120.0F;
    constexpr int HOUND_HP = 110;

    // In and out the far side of the band, at a sprint.
    const double transit_seconds = (2.0 * IMPACT_MELEE_REACH) / HOUND_SPEED;
    const int swings = static_cast<int>(transit_seconds / IMPACT_MELEE_PERIOD);
    DEFN_CHECK_EQ(swings, 2);

    // One impact cannot kill it in the window; two can. That threshold is the composition, stated as arithmetic.
    DEFN_CHECK(swings * IMPACT_MELEE_DAMAGE < HOUND_HP);
    DEFN_CHECK(2 * swings * IMPACT_MELEE_DAMAGE > HOUND_HP);

    // And it has to out-hit the thing it is taxing, or the trade is the wrong way round.
    DEFN_CHECK(IMPACT_MELEE_DAMAGE > 20); // the hound's own melee
}

// The mason's reach, pinned against the friendly roster rather than as a number on its own, because the *ordering* is
// the design and 400 is already exactly right:
//
//     breacher 245  <  impact 320  <  operator 380  <  MASON 400  <  marksman 650
//
// It out-ranges every friendly it is meant to punish, and is out-ranged by the one that is meant to answer it. The
// 250px gap to the marksman is not a shortfall -- it is what makes the counter clean: the mason spends 5.2s walking
// into its own range under fire and dies first, taking a measured **zero** off a marksman at every force size.
//
// A previous pass raised this to 600 on the strength of a probe that only ever tested the mason against a marksman.
// That is benchmarking a unit against its own counter: 600 does make it hurt marksmen, and in doing so deletes the
// matchup. Against the mass it is actually for, the shipped 400 already scales -- 102 damage per mason against two
// breachers, 263 against eight.
DEFN_TEST(mason_out_ranges_what_it_punishes_and_is_out_ranged_by_what_answers_it) {
    constexpr float MASON_REACH = 400.0F;

    DEFN_CHECK(MASON_REACH > 245.0F); // breacher
    DEFN_CHECK(MASON_REACH > 320.0F); // impact
    DEFN_CHECK(MASON_REACH > 380.0F); // operator
    DEFN_CHECK(MASON_REACH < 650.0F); // marksman -- the answer, and the only one

    // And the gap has to be wide enough to be a real counter rather than a trade: at the mason's 48px/s it is over
    // five seconds of free fire, which is more than its 82hp survives.
    constexpr float FREE_WINDOW_SECONDS = (650.0F - MASON_REACH) / 48.0F;
    DEFN_CHECK(FREE_WINDOW_SECONDS > 5.0F);
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
