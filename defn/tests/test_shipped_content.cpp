// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "data_paths.h"
#include "sim_world.h"
#include "unit_data.h"

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

} // namespace

// The mason's job, measured against **three** friendly compositions rather than one, out of the shipped catalog.
//
// Measuring it against a breacher line alone -- which is what every earlier probe did, including the table in
// `DIVERSITY_AND_BALANCE.md` 2.13.1 -- reads its best matchup and calls it the unit. The `impact` line is the same
// count of a faster, cheaper-to-lose unit, and the mixed line is the interleave of the two. Geometry, spacing, count
// and seed are identical across all three, so composition is the only variable.
//
// The numbers are pinned because each is a design statement that nothing about `affected_fraction: 1.0` and
// `splash_damage: 12` says out loud:
//
//   n=6   breacher 206 @ 4s   |   mixed 126 @ 3s   |   impact 92 @ 2s
//
// **The mason's output is set by how long it lives, not by how many bodies are in the blast.** An impact line closes
// at 98px/s and kills it in two seconds, so it fires twice; a breacher line closes at 58 and gives it twice as long.
// That is why the ordering is breacher > mixed > impact and why raising splash did not change the ordering: the
// 0.5/7 roster measured 92 / 51 / 41 in the same three cells, a uniform 2.2-2.5x below these.
DEFN_TEST(shipped_mason_output_is_ordered_by_composition_not_by_body_count) {
    UnitDataLoader catalog;
    DEFN_REQUIRE(catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS));
    const GlobalUnitConfig &globals = catalog.get_globals();

    const MasonProbe breachers = mason_against(catalog, globals, repeat("breacher", 6));
    const MasonProbe impacts = mason_against(catalog, globals, repeat("impact", 6));
    const MasonProbe mixed = mason_against(catalog, globals, interleave("breacher", "impact", 6));

    DEFN_CHECK_EQ(breachers.damage, 206);
    DEFN_CHECK_EQ(mixed.damage, 126);
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

} // namespace defn
