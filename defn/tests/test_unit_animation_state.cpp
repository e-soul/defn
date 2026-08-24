// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "unit_animation_state.h"

namespace defn {

namespace {

// Ten frames at ten frames per second: one frame every 0.1s, one full pass in 1.0s.
std::vector<std::pair<std::string, AnimConfig>> make_animations() {
    return {
        {"walk", {.path_template = "", .frame_count = 10, .speed = 10.0, .loop = true, .windup_frames = 0}},
        {"attack", {.path_template = "", .frame_count = 10, .speed = 10.0, .loop = false, .windup_frames = 3}},
        {"shoot", {.path_template = "", .frame_count = 10, .speed = 10.0, .loop = false, .windup_frames = 2}},
        {"death", {.path_template = "", .frame_count = 10, .speed = 10.0, .loop = false, .windup_frames = 0}},
    };
}

UnitAnimationState make_state() {
    UnitAnimationState state;
    state.configure(make_animations());
    state.set_pose(UnitPose::WALK);
    return state;
}

} // namespace

DEFN_TEST(unit_animation_state_starts_walking) {
    UnitAnimationState state = make_state();

    DEFN_CHECK_EQ(static_cast<int>(state.get_pose()), static_cast<int>(UnitPose::WALK));
    DEFN_CHECK_EQ(state.get_current_animation(), std::string("walk"));
    DEFN_CHECK(!state.is_attack_animation_playing());
}

DEFN_TEST(unit_animation_state_reports_the_attack_windup_and_its_end) {
    UnitAnimationState state = make_state();
    state.play_attack();

    DEFN_CHECK(state.is_attack_animation_playing());
    DEFN_CHECK(state.is_attack_windup_active());

    state.advance(0.25); // frame 2, the last committed frame
    DEFN_CHECK(state.is_attack_windup_active());

    state.advance(0.10); // frame 3, the cancelable backswing
    DEFN_CHECK(!state.is_attack_windup_active());
}

DEFN_TEST(unit_animation_state_stops_reporting_an_attack_once_it_plays_out) {
    UnitAnimationState state = make_state();
    state.play_attack();

    state.advance(0.95);
    DEFN_CHECK(state.is_attack_animation_playing());

    state.advance(0.10);
    DEFN_CHECK(!state.is_attack_animation_playing());
}

DEFN_TEST(unit_animation_state_holds_a_pose_without_running_it) {
    UnitAnimationState state = make_state();
    state.hold_pose(UnitPose::ATTACK);

    DEFN_CHECK_EQ(static_cast<int>(state.get_pose()), static_cast<int>(UnitPose::ATTACK));
    DEFN_CHECK(!state.is_attack_animation_playing());

    state.advance(1.0);
    DEFN_CHECK(!state.is_attack_animation_playing());
    DEFN_CHECK_EQ(state.get_clock().frame(), 0);
}

DEFN_TEST(unit_animation_state_releases_the_shot_on_its_spawn_frame) {
    UnitAnimationState state = make_state();

    DEFN_CHECK(!state.play_shoot(4).shoot_effect_fired);
    DEFN_CHECK(!state.consume_shoot_effect_triggered());

    DEFN_CHECK(!state.advance(0.35).shoot_effect_fired); // frame 3
    DEFN_CHECK(state.advance(0.10).shoot_effect_fired);  // frame 4

    DEFN_CHECK(state.consume_shoot_effect_triggered());
    DEFN_CHECK(!state.consume_shoot_effect_triggered());
}

DEFN_TEST(unit_animation_state_releases_a_frame_zero_shot_at_once) {
    UnitAnimationState state = make_state();

    DEFN_CHECK(state.play_shoot(0).shoot_effect_fired);
    DEFN_CHECK(state.consume_shoot_effect_triggered());
}

DEFN_TEST(unit_animation_state_releases_the_shot_at_once_without_a_shoot_animation) {
    UnitAnimationState state;
    state.configure({{"walk", {.path_template = "", .frame_count = 10, .speed = 10.0, .loop = true, .windup_frames = 0}}});

    DEFN_CHECK(state.play_shoot(4).shoot_effect_fired);
    DEFN_CHECK(state.consume_shoot_effect_triggered());
    DEFN_CHECK(!state.is_attack_animation_playing());
}

DEFN_TEST(unit_animation_state_drops_a_pending_shot_when_the_pose_changes) {
    UnitAnimationState state = make_state();
    state.play_shoot(4);
    state.cancel_pending_attack();

    DEFN_CHECK_EQ(static_cast<int>(state.get_pose()), static_cast<int>(UnitPose::WALK));
    DEFN_CHECK(!state.advance(1.0).shoot_effect_fired);
    DEFN_CHECK(!state.consume_shoot_effect_triggered());
}

DEFN_TEST(unit_animation_state_locks_the_pose_once_the_unit_dies) {
    UnitAnimationState state = make_state();
    state.set_pose(UnitPose::DEATH);

    state.set_pose(UnitPose::WALK);
    state.hold_pose(UnitPose::ATTACK);
    state.play_attack();

    DEFN_CHECK_EQ(static_cast<int>(state.get_pose()), static_cast<int>(UnitPose::DEATH));
    DEFN_CHECK_EQ(state.get_current_animation(), std::string("death"));
}

DEFN_TEST(unit_animation_state_plays_named_animations_it_knows) {
    UnitAnimationState state = make_state();

    DEFN_CHECK(!state.play_named("happy", true));
    DEFN_CHECK(state.play_named("attack", true));
    DEFN_CHECK_EQ(state.get_current_animation(), std::string("attack"));
    // A named animation is presentation only, so it never claims the pose combat reasons about.
    DEFN_CHECK_EQ(static_cast<int>(state.get_pose()), static_cast<int>(UnitPose::WALK));
}

DEFN_TEST(unit_animation_state_keeps_the_current_animation_when_the_requested_one_is_missing) {
    UnitAnimationState state;
    state.configure({{"walk", {.path_template = "", .frame_count = 10, .speed = 10.0, .loop = true, .windup_frames = 0}}});
    state.set_pose(UnitPose::WALK);

    state.play_attack();

    DEFN_CHECK_EQ(state.get_current_animation(), std::string("walk"));
    DEFN_CHECK(!state.is_attack_animation_playing());
    // The pose still changed, matching AnimationController: only the animation refuses to switch.
    DEFN_CHECK_EQ(static_cast<int>(state.get_pose()), static_cast<int>(UnitPose::ATTACK));
}

DEFN_TEST(unit_animation_state_maps_poses_to_combat_pose_states) {
    DEFN_CHECK_EQ(static_cast<int>(to_combat_pose_state(UnitPose::WALK)), static_cast<int>(CombatPoseState::WALK));
    DEFN_CHECK_EQ(static_cast<int>(to_combat_pose_state(UnitPose::ATTACK)), static_cast<int>(CombatPoseState::ATTACK));
    DEFN_CHECK_EQ(static_cast<int>(to_combat_pose_state(UnitPose::SHOOT)), static_cast<int>(CombatPoseState::SHOOT));
    DEFN_CHECK_EQ(static_cast<int>(to_combat_pose_state(UnitPose::DEATH)), static_cast<int>(CombatPoseState::OTHER));
}

} // namespace defn
