// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "spawn_timeline.h"

namespace defn {

DEFN_TEST(spawn_timeline_orders_due_spawns_and_reports_wave_changes) {
    SpawnTimeline timeline;
    timeline.load({.waves = {
                       {.wave_number = 2, .spawns = {{.time = 2.0, .type = "late"}}},
                       {.wave_number = 1, .spawns = {{.time = 0.5, .type = "first"}, {.time = 1.0, .type = "second"}}},
                   }});
    timeline.start();

    SpawnTimelineUpdate update = timeline.advance(0.75);
    DEFN_REQUIRE(update.wave_changed.has_value());
    DEFN_CHECK_EQ(*update.wave_changed, 1);
    DEFN_CHECK_EQ(update.due_spawns.size(), static_cast<size_t>(1));
    DEFN_CHECK_EQ(update.due_spawns[0].type, std::string("first"));
    DEFN_CHECK(!update.all_spawns_completed);

    update = timeline.advance(1.25);
    DEFN_REQUIRE(update.wave_changed.has_value());
    DEFN_CHECK_EQ(*update.wave_changed, 2);
    DEFN_CHECK_EQ(update.due_spawns.size(), static_cast<size_t>(2));
    DEFN_CHECK_EQ(update.due_spawns[0].type, std::string("second"));
    DEFN_CHECK_EQ(update.due_spawns[1].type, std::string("late"));
    DEFN_CHECK(update.all_spawns_completed);

    update = timeline.advance(1.0);
    DEFN_CHECK(update.due_spawns.empty());
    DEFN_CHECK(!update.all_spawns_completed);
}

DEFN_TEST(spawn_timeline_schedules_a_wave_appended_after_the_start) {
    SpawnTimeline timeline;
    timeline.load({.waves = {{.wave_number = 1, .spawns = {{.time = 1.0, .type = "first"}}}}});
    timeline.start();

    timeline.append({.wave_number = 2, .spawns = {{.time = 2.0, .type = "appended"}}});

    SpawnTimelineUpdate update = timeline.advance(1.5);
    DEFN_CHECK_EQ(update.due_spawns.size(), static_cast<size_t>(1));
    DEFN_CHECK_EQ(update.due_spawns[0].type, std::string("first"));
    DEFN_CHECK(!update.all_spawns_completed);

    update = timeline.advance(1.0);
    DEFN_REQUIRE(update.wave_changed.has_value());
    DEFN_CHECK_EQ(*update.wave_changed, 2);
    DEFN_CHECK_EQ(update.due_spawns.size(), static_cast<size_t>(1));
    DEFN_CHECK_EQ(update.due_spawns[0].type, std::string("appended"));
}

DEFN_TEST(spawn_timeline_orders_an_appended_wave_against_what_is_still_pending) {
    SpawnTimeline timeline;
    timeline.load({.waves = {{.wave_number = 1, .spawns = {{.time = 1.0, .type = "early"}, {.time = 9.0, .type = "late"}}}}});
    timeline.start();
    timeline.advance(1.5);

    // Lands between the consumed spawn and the pending one, so it has to sort into the tail rather than onto the end.
    timeline.append({.wave_number = 2, .spawns = {{.time = 4.0, .type = "between"}}});

    const SpawnTimelineUpdate update = timeline.advance(10.0);
    DEFN_CHECK_EQ(update.due_spawns.size(), static_cast<size_t>(2));
    DEFN_CHECK_EQ(update.due_spawns[0].type, std::string("between"));
    DEFN_CHECK_EQ(update.due_spawns[1].type, std::string("late"));
}

DEFN_TEST(spawn_timeline_resumes_when_a_wave_is_appended_after_the_last_spawn_was_consumed) {
    SpawnTimeline timeline;
    timeline.load({.waves = {{.wave_number = 1, .spawns = {{.time = 0.5, .type = "only"}}}}});
    timeline.start();

    SpawnTimelineUpdate update = timeline.advance(1.0);
    DEFN_CHECK(update.all_spawns_completed);
    DEFN_CHECK(timeline.all_spawns_spawned());

    timeline.append({.wave_number = 2, .spawns = {{.time = 2.0, .type = "resumed"}}});
    DEFN_CHECK(!timeline.all_spawns_spawned());

    update = timeline.advance(1.5);
    DEFN_CHECK_EQ(update.due_spawns.size(), static_cast<size_t>(1));
    DEFN_CHECK_EQ(update.due_spawns[0].type, std::string("resumed"));
    // The latch stays thrown: completion is announced once per run, and a resumed timeline has already announced it.
    DEFN_CHECK(!update.all_spawns_completed);
    DEFN_CHECK(timeline.all_spawns_spawned());
}

DEFN_TEST(spawn_timeline_starts_from_a_wave_appended_before_the_run) {
    SpawnTimeline timeline;
    timeline.load({});
    DEFN_CHECK(timeline.all_spawns_spawned());

    timeline.append({.wave_number = 1, .spawns = {{.time = 1.0, .type = "seeded"}}});
    timeline.start();

    const SpawnTimelineUpdate update = timeline.advance(1.5);
    DEFN_CHECK_EQ(update.due_spawns.size(), static_cast<size_t>(1));
    DEFN_CHECK_EQ(update.due_spawns[0].type, std::string("seeded"));
}

DEFN_TEST(spawn_timeline_does_not_advance_when_stopped) {
    SpawnTimeline timeline;
    timeline.load({.waves = {{.wave_number = 1, .spawns = {{.time = 0.0, .type = "now"}}}}});

    const SpawnTimelineUpdate before_start = timeline.advance(1.0);
    DEFN_CHECK(before_start.due_spawns.empty());

    timeline.start();
    timeline.stop();
    const SpawnTimelineUpdate after_stop = timeline.advance(1.0);
    DEFN_CHECK(after_stop.due_spawns.empty());
}

DEFN_TEST(spawn_timeline_composes_a_body_scale_with_its_waves) {
    SpawnTimeline timeline;
    SpawnTimelineDefinition definition;
    // A wave-wide ramp with one promoted body in it. The two are different granularities of the same thing, so the
    // promoted body has to carry both -- an elite that lost its wave's ramp would be the wrong body twice over.
    definition.waves.push_back({.wave_number = 1,
                                .spawns =
                                    {
                                        {.time = 0.5, .type = "grime"},
                                        {.time = 1.0, .type = "wrecker", .scale = {.hp = 3.0, .size = 1.35}},
                                    },
                                .scale = {.damage = 2.0}});
    timeline.load(definition);
    timeline.start();

    const SpawnTimelineUpdate update = timeline.advance(1.5);
    DEFN_CHECK_EQ(update.due_spawns.size(), std::size_t{2});

    DEFN_CHECK_CLOSE(update.due_spawns[0].scale.damage, 2.0, 1e-9);
    DEFN_CHECK_CLOSE(update.due_spawns[0].scale.hp, 1.0, 1e-9);

    DEFN_CHECK_CLOSE(update.due_spawns[1].scale.damage, 2.0, 1e-9);
    DEFN_CHECK_CLOSE(update.due_spawns[1].scale.hp, 3.0, 1e-9);
    DEFN_CHECK_CLOSE(update.due_spawns[1].scale.size, 1.35, 1e-9);
}

DEFN_TEST(spawn_timeline_pulls_an_idle_gap_closed_and_leaves_a_tight_one_alone) {
    SpawnTimeline timeline;
    timeline.load({.waves = {
                       {.wave_number = 1, .spawns = {{.time = 1.0, .type = "first"}, {.time = 1.5, .type = "second"}}},
                       {.wave_number = 2, .spawns = {{.time = 30.0, .type = "far"}}},
                   }});
    timeline.start();

    // Wave 1 is 0.5s apart, which is tighter than the lead: a gap that is already tight is not a gap to close, or
    // the stagger inside a wave would be flattened into the lead as well.
    DEFN_CHECK_CLOSE(timeline.pull_next_spawn_forward(2.0), 0.0, 1e-9);

    SpawnTimelineUpdate update = timeline.advance(1.6);
    DEFN_CHECK_EQ(update.due_spawns.size(), static_cast<size_t>(2));

    // 28.4s of nothing until wave 2, closed to the 2s lead.
    DEFN_CHECK_CLOSE(timeline.pull_next_spawn_forward(2.0), 26.4, 1e-9);
    DEFN_CHECK(timeline.advance(1.9).due_spawns.empty());
    update = timeline.advance(0.2);
    DEFN_REQUIRE(update.wave_changed.has_value());
    DEFN_CHECK_EQ(*update.wave_changed, 2);
    DEFN_CHECK_EQ(update.due_spawns.size(), static_cast<size_t>(1));
    DEFN_CHECK_EQ(update.due_spawns[0].type, std::string("far"));
}

DEFN_TEST(spawn_timeline_pulls_nothing_forward_with_an_empty_or_stopped_timeline) {
    SpawnTimeline timeline;
    timeline.load({.waves = {{.wave_number = 1, .spawns = {{.time = 5.0, .type = "only"}}}}});

    // Not started: the clock is not running, so there is nothing to move it against.
    DEFN_CHECK_CLOSE(timeline.pull_next_spawn_forward(1.0), 0.0, 1e-9);

    timeline.start();
    DEFN_CHECK_EQ(timeline.advance(6.0).due_spawns.size(), static_cast<size_t>(1));

    // Drained: a timeline with nothing pending has no gap, and closing one it does not have would run the clock off
    // the end of the run.
    DEFN_CHECK_CLOSE(timeline.pull_next_spawn_forward(1.0), 0.0, 1e-9);
}

} // namespace defn
