// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "damage_rules.h"
#include "data_paths.h"
#include "sim_world.h"
#include "unit_data.h"

#include <optional>
#include <string>
#include <vector>

namespace defn {

namespace {

constexpr float BELT_Y = 800.0F;
// Inside the mason's 140px blast, so the line it faces is the clustered line it is priced against.
constexpr float CLUSTER_SPACING = 60.0F;
constexpr float FRIENDLY_FRONT_X = 1000.0F;
constexpr float MASON_X = 1350.0F;

int total_damage_taken(const SimWorld &world, UnitSide side) {
    int damage = 0;
    for (const SimEntity &entity : world.get_entities()) {
        if (entity.side == side) {
            damage += entity.damage_taken;
        }
    }

    return damage;
}

// One mason against a named friendly line, read out of the shipped catalog rather than a copy of it. Composition is
// the only variable: the count, the spacing and the geometry are identical for every mix.
struct MasonProbe {
    int damage = 0;
    double seconds = 0.0;
    bool mason_died = false;
};

MasonProbe mason_against(const UnitCatalog &catalog, const GlobalUnitConfig &globals, const std::vector<std::string> &line) {
    StdRandomSource random(2026U);
    SimWorld world(catalog, globals, random);

    for (std::size_t index = 0; index < line.size(); ++index) {
        world.spawn(line[index], UnitSide::FRIENDLY, {.x = FRIENDLY_FRONT_X - (CLUSTER_SPACING * static_cast<float>(index)), .y = BELT_Y});
    }
    world.spawn("mason", UnitSide::HOSTILE, {.x = MASON_X, .y = BELT_Y});

    const SimEngagementReport report = run_engagement(world, 60.0);
    return {.damage = total_damage_taken(world, UnitSide::FRIENDLY), .seconds = report.duration_seconds, .mason_died = report.hostile.alive == 0};
}

// Round-robin, so a mixed line is genuinely interleaved rather than one block behind another -- the same rule the
// engagement lab uses, and for the same reason: placement decides who trades first.
std::vector<std::string> interleave(const std::string &first, const std::string &second, int total) {
    std::vector<std::string> line;
    line.reserve(static_cast<std::size_t>(total));
    for (int index = 0; index < total; ++index) {
        line.push_back(index % 2 == 0 ? first : second);
    }
    return line;
}

std::vector<std::string> repeat(const std::string &unit_id, int total) { return std::vector<std::string>(static_cast<std::size_t>(total), unit_id); }

// One friendly against three hostiles in a line. The geometry is the native suite's old probe, kept so the two are
// comparable; what changed is that the units are now the shipped ones rather than a hand-written copy of them.
struct LineProbe {
    bool resolved = false;
    std::optional<UnitSide> winner;
    int friendly_alive = 0;
    int friendly_hp_remaining = 0;
    int friendly_damage_dealt = 0;
    int hostile_alive = 0;
    int hostile_damage_dealt = 0;
    double seconds = 0.0;
};

LineProbe three_against(const UnitCatalog &catalog, const GlobalUnitConfig &globals, const std::string &friendly_id, const std::string &hostile_id) {
    StdRandomSource random(2026U);
    SimWorld world(catalog, globals, random);

    world.spawn(friendly_id, UnitSide::FRIENDLY, {.x = FRIENDLY_FRONT_X, .y = BELT_Y});
    world.spawn(hostile_id, UnitSide::HOSTILE, {.x = 1500.0F, .y = BELT_Y});
    world.spawn(hostile_id, UnitSide::HOSTILE, {.x = 1650.0F, .y = BELT_Y});
    world.spawn(hostile_id, UnitSide::HOSTILE, {.x = 1800.0F, .y = BELT_Y});

    const SimEngagementReport report = run_engagement(world, 60.0);
    return {.resolved = report.resolved,
            .winner = report.winner,
            .friendly_alive = report.friendly.alive,
            .friendly_hp_remaining = report.friendly.hp_remaining,
            .friendly_damage_dealt = report.friendly.damage_dealt,
            .hostile_alive = report.hostile.alive,
            .hostile_damage_dealt = report.hostile.damage_dealt,
            .seconds = report.duration_seconds};
}

} // namespace

// The mason's job, measured against **three** friendly compositions rather than one, out of the shipped catalog.
//
// Measuring it against a breacher line alone -- which is what every earlier probe did -- reads its best matchup and
// calls it the unit. The `impact` line is the same count of a faster, cheaper-to-lose unit, and the mixed line is the
// interleave of the two. Geometry, spacing, count and seed are identical across all three, so composition is the only
// variable.
//
// The numbers are pinned because each is a design statement that nothing about `affected_fraction: 1.0` and
// `splash_damage: 12` says out loud:
//
//   n=6   breacher 134 @ 4s   |   mixed 102 @ 3s   |   impact 92 @ 2s
//
// **The mason's output is set by how long it lives, not by how many bodies are in the blast.** An impact line closes
// at 98px/s and kills it in two seconds, so it fires twice; a breacher line closes at 58 and gives it twice as long.
// That is why the ordering is breacher > mixed > impact, and the ordering has survived two content changes that
// moved every absolute number in it: the 0.5/7 mason measured 92 / 51 / 41 here, the 1.0/12 mason 206 / 126 / 92,
// and the breacher's `armour: 4` then took the first two down to these by blunting splash 12 -> 8 and the direct
// hit 10 -> 6. The impact column is untouched at 92 across that last change, because `impact` carries no armour --
// which is the cleanest single demonstration in the suite that armour is what moved the other two.
DEFN_TEST(shipped_mason_output_is_ordered_by_composition_not_by_body_count) {
    UnitDataLoader catalog;
    DEFN_REQUIRE(catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS));
    const GlobalUnitConfig &globals = catalog.get_globals();

    const MasonProbe breachers = mason_against(catalog, globals, repeat("breacher", 6));
    const MasonProbe impacts = mason_against(catalog, globals, repeat("impact", 6));
    const MasonProbe mixed = mason_against(catalog, globals, interleave("breacher", "impact", 6));

    DEFN_CHECK_EQ(breachers.damage, 134);
    DEFN_CHECK_EQ(mixed.damage, 102);
    DEFN_CHECK_EQ(impacts.damage, 92);

    // The ordering is the design; the absolutes above only pin where it currently sits. A mason that stopped caring
    // which of these it faced would have lost the job it was repriced to do.
    DEFN_CHECK(breachers.damage > mixed.damage);
    DEFN_CHECK(mixed.damage > impacts.damage);

    // And it is survival time that orders them, not the blast.
    DEFN_CHECK(breachers.seconds > mixed.seconds);
    DEFN_CHECK(mixed.seconds > impacts.seconds);
    DEFN_CHECK(breachers.mason_died && mixed.mason_died && impacts.mason_died);
}

// One mason saturates at six defenders: it is dead before the seventh and eighth ever matter. Recorded because
// 2.13.1's "102 -> 263 as the force grows" is a *per-mason* number from a force of masons, and it is easy to carry
// that curve over to a single shell and conclude the unit scales without limit. It does not.
DEFN_TEST(shipped_mason_output_saturates_once_it_dies_before_the_back_rank) {
    UnitDataLoader catalog;
    DEFN_REQUIRE(catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS));
    const GlobalUnitConfig &globals = catalog.get_globals();

    for (const std::string &unit_id : {std::string("breacher"), std::string("impact")}) {
        const MasonProbe four = mason_against(catalog, globals, repeat(unit_id, 4));
        const MasonProbe six = mason_against(catalog, globals, repeat(unit_id, 6));
        const MasonProbe eight = mason_against(catalog, globals, repeat(unit_id, 8));

        DEFN_CHECK(six.damage > four.damage);
        DEFN_CHECK_EQ(six.damage, eight.damage);
    }
}

// The counter, catalog-true and across the same three compositions: whatever the line is made of, a marksman in it
// opens at 650 while the mason answers at 400, and the 250px walk at 48px/s is longer than 82hp lasts. Splash is paid
// only to targets that are not the direct one, so neither lever can reach this.
DEFN_TEST(shipped_marksman_answers_the_mason_for_free) {
    UnitDataLoader catalog;
    DEFN_REQUIRE(catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS));

    StdRandomSource random(2026U);
    SimWorld world(catalog, catalog.get_globals(), random);
    world.spawn("marksman", UnitSide::FRIENDLY, {.x = FRIENDLY_FRONT_X, .y = BELT_Y});
    world.spawn("mason", UnitSide::HOSTILE, {.x = FRIENDLY_FRONT_X + 800.0F, .y = BELT_Y});

    const SimEngagementReport report = run_engagement(world, 60.0);

    DEFN_REQUIRE(report.winner.has_value());
    DEFN_CHECK_EQ(static_cast<int>(*report.winner), static_cast<int>(UnitSide::FRIENDLY));
    DEFN_CHECK_EQ(total_damage_taken(world, UnitSide::FRIENDLY), 0);
}

// The armour profile, pinned as the counter-relationship it encodes rather than as four numbers.
//
// Armour subtracts flatly, so its value is set entirely by the *shot* it meets: it is worth little against a heavy
// round and nearly everything against a stream of small ones. That is the only property in the catalog whose payoff
// depends on which units are facing it, which is why it is the one lever that moved the matchup term `Var(R)` --
// speed and health, tried on four units each, moved only the power term.
//
// Two relationships have to hold, and they run in opposite directions:
//
//   friendly armour answers volume  -- the breacher's 4 takes grime's rifle to the floor, which is what pays for
//                                      grime's own armour on level 1, where grime is the entire threat
//   hostile armour is answered by burst -- grime's 4 halves the operator's shot and costs the marksman a fifth of
//                                      its own, so the marksman is the answer and the operator is the victim
//
// Both are breakpoints. Nothing about `4` says either of them, so they are said here.
DEFN_TEST(shipped_armour_answers_volume_and_is_answered_by_burst) {
    UnitDataLoader catalog;
    DEFN_REQUIRE(catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS));

    const auto unit = [&catalog](const char *unit_id) {
        const auto found = catalog.get_unit(unit_id);
        DEFN_REQUIRE(found.has_value());
        return *found;
    };
    const UnitConfig breacher = unit("breacher");
    const UnitConfig marksman = unit("marksman");
    const UnitConfig gunner = unit("operator");
    const UnitConfig grime = unit("grime");

    // A tank's armour is worth more than a sniper's, or armour is just health with extra steps.
    DEFN_CHECK_EQ(breacher.armour, 4);
    DEFN_CHECK_EQ(marksman.armour, 0);
    DEFN_CHECK_EQ(grime.armour, 4);

    // Friendly side: the breacher takes grime's rifle to the floor. This is the whole of the level-1 answer.
    DEFN_CHECK_EQ(damage_after_armour(grime.ranged_damage, breacher.armour), 1);
    DEFN_CHECK(damage_after_armour(grime.ranged_damage, marksman.armour) > damage_after_armour(grime.ranged_damage, breacher.armour));

    // Hostile side: the same armour value costs the volume shooter far more of its shot than the burst one.
    const double operator_kept = static_cast<double>(damage_after_armour(gunner.ranged_damage, grime.armour)) / gunner.ranged_damage;
    const double marksman_kept = static_cast<double>(damage_after_armour(marksman.ranged_damage, grime.armour)) / marksman.ranged_damage;
    DEFN_CHECK(operator_kept < 0.5);  // 6 -> 2
    DEFN_CHECK(marksman_kept > 0.75); // 19 -> 15
    DEFN_CHECK(marksman_kept > operator_kept * 2.0);
}

// The smallest question the engagement lab exists to answer: how does one roster entry fare against a known threat?
//
// It is pinned here, against the real catalog, because the native suite answered it **backwards** for as long as it
// answered it at all. That suite cannot read `res://`, so it carried a hand-written roster which omitted `armour` --
// and armour is the whole of this matchup. Grime's rifle does 5, so the breacher's 4 takes it to the floor of 1 and
// three grime need four hundred shots to bring it down; the marksman carries no armour, takes the full 5 from each,
// and dies with two of them still standing.
//
// So this is the level-1 answer stated as an outcome rather than as a damage formula: **the cheap anchor is the
// counter to massed grime and the expensive sniper is not**, which is the reverse of what the old pins asserted.
//
// The breacher's match runs to 57.3s against a 60s cap. If a rules change pushes it past, this fails on `resolved`
// rather than on the winner -- read that as the cap running out, not as the breacher losing.
DEFN_TEST(shipped_breacher_answers_three_grime_and_the_marksman_does_not) {
    UnitDataLoader catalog;
    DEFN_REQUIRE(catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS));
    const GlobalUnitConfig &globals = catalog.get_globals();

    const LineProbe breacher = three_against(catalog, globals, "breacher", "grime");
    const LineProbe marksman = three_against(catalog, globals, "marksman", "grime");

    DEFN_REQUIRE(breacher.resolved);
    DEFN_REQUIRE(breacher.winner.has_value());
    DEFN_CHECK_EQ(static_cast<int>(*breacher.winner), static_cast<int>(UnitSide::FRIENDLY));
    DEFN_CHECK_EQ(breacher.hostile_alive, 0);
    DEFN_CHECK_EQ(breacher.friendly_damage_dealt, 288); // three grime at 96 each, with nothing wasted on overkill
    DEFN_CHECK_EQ(breacher.friendly_hp_remaining, 227); // and it clears them on more than half its pool
    DEFN_CHECK_CLOSE(breacher.seconds, 57.3, 1.0);

    DEFN_REQUIRE(marksman.resolved);
    DEFN_REQUIRE(marksman.winner.has_value());
    DEFN_CHECK_EQ(static_cast<int>(*marksman.winner), static_cast<int>(UnitSide::HOSTILE));
    DEFN_CHECK_EQ(marksman.friendly_alive, 0);
    DEFN_CHECK_EQ(marksman.hostile_alive, 2); // it trades itself for exactly one of them
    DEFN_CHECK_CLOSE(marksman.seconds, 12.7, 1.0);

    // The comparison is the design statement; the absolutes above only pin where it currently sits.
    DEFN_CHECK(breacher.seconds > marksman.seconds * 3.0);
}

// The same breacher against the same count of a different threat. It clears both, so the comparable number is what
// each one charged it: the mason takes roughly twice the health and settles the fight in half the time, because
// splash 12 against armour 4 still lands 8 where grime's rifle lands the floor of 1. The threat ratio the design
// used to estimate, measured -- and measured against a mason the breacher can actually be hurt by.
DEFN_TEST(shipped_mason_costs_the_breacher_twice_what_grime_does) {
    UnitDataLoader catalog;
    DEFN_REQUIRE(catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS));
    const GlobalUnitConfig &globals = catalog.get_globals();

    const LineProbe versus_grime = three_against(catalog, globals, "breacher", "grime");
    const LineProbe versus_mason = three_against(catalog, globals, "breacher", "mason");

    DEFN_REQUIRE(versus_grime.winner.has_value());
    DEFN_REQUIRE(versus_mason.winner.has_value());
    DEFN_CHECK_EQ(static_cast<int>(*versus_grime.winner), static_cast<int>(UnitSide::FRIENDLY));
    DEFN_CHECK_EQ(static_cast<int>(*versus_mason.winner), static_cast<int>(UnitSide::FRIENDLY));

    DEFN_CHECK_EQ(versus_grime.hostile_damage_dealt, 173);
    DEFN_CHECK_EQ(versus_mason.hostile_damage_dealt, 336);
    DEFN_CHECK(static_cast<double>(versus_mason.hostile_damage_dealt) > static_cast<double>(versus_grime.hostile_damage_dealt) * 1.5);

    // And it gets there in half the time, off a body carrying 82hp against grime's 96.
    DEFN_CHECK_CLOSE(versus_mason.seconds, 30.4, 1.0);
    DEFN_CHECK(versus_mason.seconds < versus_grime.seconds);
}

} // namespace defn
