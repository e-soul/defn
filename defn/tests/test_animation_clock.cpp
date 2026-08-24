// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "animation_clock.h"

namespace defn {

namespace {

// Ten frames at ten frames per second: one frame every 0.1s, one full pass in 1.0s.
AnimConfig make_anim(bool loop = false, int windup_frames = 3) {
    return AnimConfig{.path_template = "", .frame_count = 10, .speed = 10.0, .loop = loop, .windup_frames = windup_frames};
}

} // namespace

DEFN_TEST(animation_clock_starts_on_the_first_frame_and_plays) {
    AnimationClock clock;
    clock.play(make_anim());

    DEFN_CHECK(clock.has_animation());
    DEFN_CHECK(clock.is_playing());
    DEFN_CHECK_EQ(clock.frame(), 0);
}

DEFN_TEST(animation_clock_advances_one_frame_per_frame_duration) {
    AnimationClock clock;
    clock.play(make_anim());

    clock.advance(0.05);
    DEFN_CHECK_EQ(clock.frame(), 0);

    clock.advance(0.06);
    DEFN_CHECK_EQ(clock.frame(), 1);

    clock.advance(0.10);
    DEFN_CHECK_EQ(clock.frame(), 2);

    clock.advance(0.40);
    DEFN_CHECK_EQ(clock.frame(), 6);
    DEFN_CHECK(clock.is_playing());
}

DEFN_TEST(animation_clock_windup_covers_the_leading_frames_only) {
    AnimationClock clock;
    clock.play(make_anim(false, 3));

    DEFN_CHECK(clock.is_windup_active());

    clock.advance(0.25); // frame 2, the last committed frame
    DEFN_CHECK_EQ(clock.frame(), 2);
    DEFN_CHECK(clock.is_windup_active());

    clock.advance(0.10); // frame 3, the backswing starts
    DEFN_CHECK_EQ(clock.frame(), 3);
    DEFN_CHECK(!clock.is_windup_active());
}

DEFN_TEST(animation_clock_without_windup_frames_is_never_committed) {
    AnimationClock clock;
    clock.play(make_anim(false, 0));

    DEFN_CHECK(clock.is_playing());
    DEFN_CHECK(!clock.is_windup_active());
}

DEFN_TEST(animation_clock_clamps_and_stops_a_non_looping_animation) {
    AnimationClock clock;
    clock.play(make_anim());

    clock.advance(0.95);
    DEFN_CHECK_EQ(clock.frame(), 9);
    DEFN_CHECK(clock.is_playing());

    clock.advance(0.10); // the last frame has now been shown for its full duration
    DEFN_CHECK_EQ(clock.frame(), 9);
    DEFN_CHECK(!clock.is_playing());
    DEFN_CHECK(!clock.is_windup_active());

    clock.advance(5.0);
    DEFN_CHECK_EQ(clock.frame(), 9);
    DEFN_CHECK(!clock.is_playing());
}

DEFN_TEST(animation_clock_wraps_a_looping_animation) {
    AnimationClock clock;
    clock.play(make_anim(true));

    clock.advance(0.60);
    clock.advance(0.65); // 1.25s: a quarter of the way into the second pass
    DEFN_CHECK_EQ(clock.frame(), 2);
    DEFN_CHECK(clock.is_playing());

    clock.advance(100.0);
    DEFN_CHECK(clock.is_playing());
    DEFN_CHECK(clock.frame() >= 0);
    DEFN_CHECK(clock.frame() < 10);
}

DEFN_TEST(animation_clock_hold_poses_the_first_frame_without_running_it) {
    AnimationClock clock;
    clock.play(make_anim());
    clock.advance(0.35);
    DEFN_CHECK_EQ(clock.frame(), 3);

    clock.hold(make_anim());
    DEFN_CHECK(clock.has_animation());
    DEFN_CHECK(!clock.is_playing());
    DEFN_CHECK(!clock.is_windup_active());
    DEFN_CHECK_EQ(clock.frame(), 0);

    clock.advance(0.50);
    DEFN_CHECK_EQ(clock.frame(), 0);
    DEFN_CHECK(!clock.is_playing());
}

DEFN_TEST(animation_clock_play_always_restarts_from_the_first_frame) {
    AnimationClock clock;
    clock.play(make_anim());
    clock.advance(0.55);
    DEFN_CHECK_EQ(clock.frame(), 5);

    clock.play(make_anim());
    DEFN_CHECK_EQ(clock.frame(), 0);
    DEFN_CHECK(clock.is_playing());
}

DEFN_TEST(animation_clock_resume_continues_where_it_stopped) {
    AnimationClock clock;
    clock.play(make_anim());
    clock.advance(0.35);
    clock.stop();
    DEFN_CHECK(!clock.is_playing());

    clock.resume(make_anim());
    DEFN_CHECK(clock.is_playing());
    DEFN_CHECK_EQ(clock.frame(), 3);

    clock.advance(0.10);
    DEFN_CHECK_EQ(clock.frame(), 4);
}

DEFN_TEST(animation_clock_resume_restarts_an_animation_that_already_played_out) {
    AnimationClock clock;
    clock.play(make_anim());
    clock.advance(2.0);
    DEFN_CHECK(!clock.is_playing());

    clock.resume(make_anim());
    DEFN_CHECK(clock.is_playing());
    DEFN_CHECK_EQ(clock.frame(), 0);
}

DEFN_TEST(animation_clock_resume_runs_a_held_animation_from_its_first_frame) {
    AnimationClock clock;
    clock.hold(make_anim());

    clock.resume(make_anim());
    DEFN_CHECK(clock.is_playing());
    DEFN_CHECK_EQ(clock.frame(), 0);

    clock.advance(0.15);
    DEFN_CHECK_EQ(clock.frame(), 1);
}

DEFN_TEST(animation_clock_ignores_an_animation_without_frames) {
    AnimationClock clock;
    clock.play(AnimConfig{.path_template = "", .frame_count = 0, .speed = 10.0, .loop = false, .windup_frames = 0});

    DEFN_CHECK(!clock.has_animation());
    DEFN_CHECK(!clock.is_playing());
    DEFN_CHECK_EQ(clock.frame(), 0);
}

DEFN_TEST(animation_clock_keeps_the_current_animation_when_asked_to_play_an_empty_one) {
    AnimationClock clock;
    clock.play(make_anim());
    clock.advance(0.35);

    clock.play(AnimConfig{.path_template = "", .frame_count = 0, .speed = 10.0, .loop = false, .windup_frames = 0});

    DEFN_CHECK(clock.is_playing());
    DEFN_CHECK_EQ(clock.frame(), 3);
}

DEFN_TEST(animation_clock_never_advances_a_zero_speed_animation) {
    AnimationClock clock;
    clock.play(AnimConfig{.path_template = "", .frame_count = 10, .speed = 0.0, .loop = false, .windup_frames = 3});

    clock.advance(5.0);
    DEFN_CHECK_EQ(clock.frame(), 0);
    DEFN_CHECK(clock.is_playing());
    DEFN_CHECK(clock.is_windup_active());
}

DEFN_TEST(animation_clock_ignores_non_positive_deltas) {
    AnimationClock clock;
    clock.play(make_anim());
    clock.advance(0.35);

    clock.advance(0.0);
    clock.advance(-1.0);
    DEFN_CHECK_EQ(clock.frame(), 3);
    DEFN_CHECK_CLOSE(clock.elapsed_seconds(), 0.35, 0.0001);
}

DEFN_TEST(animation_clock_exposes_the_animation_it_is_running) {
    AnimationClock clock;
    clock.play(make_anim(true, 4));

    DEFN_CHECK_EQ(clock.config().frame_count, 10);
    DEFN_CHECK_EQ(clock.config().windup_frames, 4);
    DEFN_CHECK(clock.config().loop);
}

} // namespace defn
