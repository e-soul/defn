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