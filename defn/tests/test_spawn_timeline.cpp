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

} // namespace defn