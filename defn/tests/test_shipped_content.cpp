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

// Who beats whom: one friendly against a short hostile line, out of the shipped catalog. The same 150px spacing as
// `three_against`, with the count a parameter, so a pin here is a claim about one matchup and nothing else. The cap
// is generous because a duel that both sides survive for a long time is itself a reading.
struct Duel {
    bool resolved = false;
    bool friendly_won = false;
    int friendly_damage_taken = 0;
    int hostile_damage_taken = 0;
    double seconds = 0.0;
};

Duel duel(const UnitCatalog &catalog, const GlobalUnitConfig &globals, const std::string &friendly_id, const std::string &hostile_id, int hostiles) {
    StdRandomSource random(2026U);
    SimWorld world(catalog, globals, random);
    world.spawn(friendly_id, UnitSide::FRIENDLY, {.x = FRIENDLY_FRONT_X, .y = BELT_Y});
    for (int index = 0; index < hostiles; ++index) {
        world.spawn(hostile_id, UnitSide::HOSTILE, {.x = 1500.0F + (150.0F * static_cast<float>(index)), .y = BELT_Y});
    }

    const SimEngagementReport report = run_engagement(world, 90.0);
    return {.resolved = report.resolved,
            .friendly_won = report.winner.has_value() && *report.winner == UnitSide::FRIENDLY,
            .friendly_damage_taken = total_damage_taken(world, UnitSide::FRIENDLY),
            .hostile_damage_taken = total_damage_taken(world, UnitSide::HOSTILE),
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
// **The mason's output is set by how long it lives, not by how many bodies are in the blast.** An impact line closes
// at 98px/s and kills it in a couple of seconds; a breacher line closes at 58 and gives it twice as long. That is why
// the ordering is breacher > mixed > impact, and the ordering has now survived three content changes that moved
// every absolute number in it: the 0.5/7 mason measured 92 / 51 / 41 here, the 1.0/12 mason 206 / 126 / 92, the
// breacher's `armour: 4` took the first two to 134 / 102 / 92, and the impact's evasion then took the last two to
// 72 / 48 -- a rocket's 10 and its 12 splash both arrive as 6 against a cap of 6, while the breacher column is
// untouched at 134, which is the cleanest demonstration in the suite that the cap is what moved the other two.
DEFN_TEST(shipped_mason_output_is_ordered_by_composition_not_by_body_count) {
    UnitDataLoader catalog;
    DEFN_REQUIRE(catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS));
    const GlobalUnitConfig &globals = catalog.get_globals();

    const MasonProbe breachers = mason_against(catalog, globals, repeat("breacher", 6));
    const MasonProbe impacts = mason_against(catalog, globals, repeat("impact", 6));
    const MasonProbe mixed = mason_against(catalog, globals, interleave("breacher", "impact", 6));
    DEFN_CHECK_EQ(breachers.damage, 134);
    DEFN_CHECK_EQ(mixed.damage, 72);
    DEFN_CHECK_EQ(impacts.damage, 48);

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

// Every unit carries one profile or neither, never both. The mason is plain on purpose: its answer is reach.
DEFN_TEST(shipped_every_unit_carries_one_mitigation_profile_or_neither) {
    UnitDataLoader catalog;
    DEFN_REQUIRE(catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS));

    struct Expected {
        const char *unit_id;
        int armour;
        int damage_cap;
    };
    const std::vector<Expected> expectations = {
        {.unit_id = "breacher", .armour = 4, .damage_cap = 0}, {.unit_id = "wrecker", .armour = 4, .damage_cap = 0},
        {.unit_id = "jackal", .armour = 4, .damage_cap = 0},   {.unit_id = "grime", .armour = 0, .damage_cap = 6},
        {.unit_id = "hound", .armour = 0, .damage_cap = 6},    {.unit_id = "impact", .armour = 0, .damage_cap = 6},
        {.unit_id = "marksman", .armour = 0, .damage_cap = 0}, {.unit_id = "operator", .armour = 0, .damage_cap = 0},
        {.unit_id = "mason", .armour = 0, .damage_cap = 0},
    };
    for (const Expected &expected : expectations) {
        const auto config = catalog.get_unit(expected.unit_id);
        DEFN_REQUIRE(config.has_value());
        DEFN_CHECK_EQ(config->armour, expected.armour);
        DEFN_CHECK_EQ(config->damage_cap, expected.damage_cap);
    }
}

// The roster's one axis, pinned as the pair of counter-relationships it encodes rather than as nine numbers.
//
// Armour subtracts per hit, so it costs a stream of light rounds nearly everything and a heavy round almost nothing.
// The cap truncates per hit, so it costs the heavy round most of its weight and leaves the light round whole. One
// profile on each hostile puts every question at one end of that axis or the other, and the two friendly guns sit at
// the two ends: the marksman is the answer to armour and the victim of evasion, the operator the reverse. Without
// the second end every armour value on the board asked for burst, which is how reach-and-burst came to answer every
// question at once.
//
// The cap reads the delivery and armour does not: a swing in contact lands whole on an evasive target. That is the
// clause that lets the same stat give the sniper a weakness without deleting the counter-puncher's job.
DEFN_TEST(shipped_armour_is_answered_by_burst_and_evasion_by_volume) {
    UnitDataLoader catalog;
    DEFN_REQUIRE(catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS));

    const auto marksman = catalog.get_unit("marksman");
    const auto gunner = catalog.get_unit("operator");
    const auto impact = catalog.get_unit("impact");
    const auto hound = catalog.get_unit("hound");
    DEFN_REQUIRE(marksman.has_value() && gunner.has_value() && impact.has_value() && hound.has_value());

    const auto kept = [](int shot, int armour, int cap) {
        return static_cast<double>(damage_after_mitigation(shot, cap, armour, DamageDelivery::RANGED)) / static_cast<double>(shot);
    };
    // Against armour the heavy round keeps most of itself and the light one a third; against the cap the light round
    // keeps all of itself and the heavy one a third. The two orderings are inverses, which is the whole design.
    DEFN_CHECK(kept(marksman->ranged_damage, 4, 0) > 0.75); // 19 -> 15
    DEFN_CHECK(kept(gunner->ranged_damage, 4, 0) < 0.4);    // 6 -> 2
    DEFN_CHECK(kept(marksman->ranged_damage, 0, 6) < 0.4);  // 19 -> 6
    DEFN_CHECK_EQ(kept(gunner->ranged_damage, 0, 6), 1.0);  // 6 -> 6
    DEFN_CHECK(kept(marksman->ranged_damage, 4, 0) > kept(gunner->ranged_damage, 4, 0) * 2.0);
    DEFN_CHECK(kept(gunner->ranged_damage, 0, 6) > kept(marksman->ranged_damage, 0, 6) * 2.0);

    // The swing ignores the cap. The impact's contact hit is the heaviest single hit in the roster, and it lands
    // whole on the hound it exists to stop.
    DEFN_CHECK_EQ(damage_after_mitigation(impact->melee_damage, hound->damage_cap, hound->armour, DamageDelivery::MELEE), impact->melee_damage);
    DEFN_CHECK(impact->melee_damage > hound->damage_cap * 4);
}

// The swarm question: a pack of grime, the cheapest hostile, against each friendly gun alone.
//
// This used to read the other way -- the breacher was the answer because grime's 5-damage rifle met its armour at the
// floor, and the marksman lost because it carried none. The grime is evasive now: a marksman's 19 arrives as 6, so a
// sniper needs sixteen rounds per body and dies to a pair of them. The impact's 8 is truncated to 6 as well; it
// beats the pair, but only because it is evasive itself and its melee swing is whole, and it walks away with a
// seventh of its health where the operator keeps well over a third. Against three the numbers are past any one gun,
// so what is pinned there is the ordering of what each managed: **volume answers the swarm and burst does not**,
// which is the operator's job stated as an outcome.
DEFN_TEST(shipped_operator_answers_a_grime_pack_and_burst_does_not) {
    UnitDataLoader catalog;
    DEFN_REQUIRE(catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS));
    const GlobalUnitConfig &globals = catalog.get_globals();

    const Duel gunner = duel(catalog, globals, "operator", "grime", 2);
    const Duel marksman = duel(catalog, globals, "marksman", "grime", 2);
    const Duel impact = duel(catalog, globals, "impact", "grime", 2);

    DEFN_REQUIRE(gunner.resolved && marksman.resolved && impact.resolved);
    DEFN_CHECK(gunner.friendly_won);
    DEFN_CHECK(!marksman.friendly_won);
    DEFN_CHECK(impact.friendly_won);
    DEFN_CHECK_EQ(gunner.friendly_damage_taken, 133); // of 225
    DEFN_CHECK_EQ(impact.friendly_damage_taken, 210); // of 245
    DEFN_CHECK(gunner.seconds < impact.seconds);

    // Three is past any one gun, and the ordering of what each one did before it went down is the same ordering.
    const Duel gunner_three = duel(catalog, globals, "operator", "grime", 3);
    const Duel marksman_three = duel(catalog, globals, "marksman", "grime", 3);
    const Duel impact_three = duel(catalog, globals, "impact", "grime", 3);
    DEFN_CHECK_EQ(gunner_three.hostile_damage_taken, 240);  // two and a half of the three
    DEFN_CHECK_EQ(impact_three.hostile_damage_taken, 132);  // one and a third
    DEFN_CHECK_EQ(marksman_three.hostile_damage_taken, 54); // nine capped rounds, not one body
    DEFN_CHECK(gunner_three.hostile_damage_taken > impact_three.hostile_damage_taken);
    DEFN_CHECK(impact_three.hostile_damage_taken > marksman_three.hostile_damage_taken * 2);
}

// The bruiser question: one armoured wrecker against each friendly gun alone. Burst answers it from either end of the
// field -- the marksman before it can shoot back, the impact by walking into its face and out-trading it there,
// because a 13-damage round arrives as 6 against the impact's cap while the shotgun's 16 arrives as 12 against
// armour 4. The operator's 6 arrives as 2, and it is dead long before the wrecker is.
DEFN_TEST(shipped_burst_answers_the_wrecker_and_volume_does_not) {
    UnitDataLoader catalog;
    DEFN_REQUIRE(catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS));
    const GlobalUnitConfig &globals = catalog.get_globals();

    const Duel marksman = duel(catalog, globals, "marksman", "wrecker", 1);
    const Duel impact = duel(catalog, globals, "impact", "wrecker", 1);
    const Duel gunner = duel(catalog, globals, "operator", "wrecker", 1);
    DEFN_REQUIRE(marksman.resolved && impact.resolved && gunner.resolved);
    DEFN_CHECK(marksman.friendly_won);
    DEFN_CHECK(impact.friendly_won);
    DEFN_CHECK(!gunner.friendly_won);

    // The marksman opens at 650 against the wrecker's 330 and lands free rounds on the walk in, so it is the cheaper
    // of the two answers in health.
    DEFN_CHECK(marksman.friendly_damage_taken < impact.friendly_damage_taken);
}

// The diver question. The hound is evasive, so a sniper's approach fire arrives as 6 per round and it reaches the
// line with most of its 110hp; the marksman's own swing is 8, and it loses in contact. Everything else on the
// friendly roster beats a lone hound: the operator with volume on the way in, the impact with a 30-damage swing the
// cap cannot touch, the breacher by simply outlasting it in contact. **The hound is the marksman's question and
// nobody else's**, which is what makes a sniper line something the player has to cover rather than something that
// covers itself.
DEFN_TEST(shipped_hound_beats_a_lone_marksman_and_loses_to_every_other_friendly) {
    UnitDataLoader catalog;
    DEFN_REQUIRE(catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS));
    const GlobalUnitConfig &globals = catalog.get_globals();

    const Duel marksman = duel(catalog, globals, "marksman", "hound", 1);
    DEFN_REQUIRE(marksman.resolved);
    DEFN_CHECK(!marksman.friendly_won);
    for (const char *answer : {"operator", "impact", "breacher"}) {
        const Duel outcome = duel(catalog, globals, answer, "hound", 1);
        DEFN_REQUIRE(outcome.resolved);
        DEFN_CHECK(outcome.friendly_won);
    }

    // And the impact is the *fast* answer: it takes the hound apart in a handful of swings, where the breacher grinds.
    DEFN_CHECK(duel(catalog, globals, "impact", "hound", 1).seconds < duel(catalog, globals, "breacher", "hound", 1).seconds);
}

// The same breacher against the same count of two threats, both cleared. This used to read "the mason costs the
// breacher twice what grime does", because grime's 5-damage rifle landed the floor of 1 through armour 4 where the
// rocket's splash landed 8. Grime's rifle is 7 now and lands 3, so three of them charge the breacher about what three
// masons do -- and take a third longer to be cleared, because the breacher's own 8-round arrives as 6 against their
// cap. **The cheapest hostile is no longer a free kill for the cheapest friendly**; it is merely the one the breacher
// can afford to stand in front of while something with volume does the killing.
DEFN_TEST(shipped_breacher_clears_three_grime_and_three_masons_at_a_similar_price) {
    UnitDataLoader catalog;
    DEFN_REQUIRE(catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS));
    const GlobalUnitConfig &globals = catalog.get_globals();

    const LineProbe versus_grime = three_against(catalog, globals, "breacher", "grime");
    const LineProbe versus_mason = three_against(catalog, globals, "breacher", "mason");

    DEFN_REQUIRE(versus_grime.winner.has_value());
    DEFN_REQUIRE(versus_mason.winner.has_value());
    DEFN_CHECK_EQ(static_cast<int>(*versus_grime.winner), static_cast<int>(UnitSide::FRIENDLY));
    DEFN_CHECK_EQ(static_cast<int>(*versus_mason.winner), static_cast<int>(UnitSide::FRIENDLY));

    DEFN_CHECK_EQ(versus_grime.hostile_damage_dealt, 354);
    DEFN_CHECK_EQ(versus_mason.hostile_damage_dealt, 336);
    DEFN_CHECK_CLOSE(versus_grime.seconds, 39.7, 1.0);
    DEFN_CHECK_CLOSE(versus_mason.seconds, 30.4, 1.0);
    DEFN_CHECK(versus_mason.seconds < versus_grime.seconds);
}

// The depth axis is opt-in, and the hound is the only unit that has opted in. Pinned as a pair, because either half
// alone is a weaker claim: a hound that lost its rate would run at a sniper and bite the air a lane above it, and a
// catalog that quietly handed the rate to everything would make every line converge into a single file.
//
// The rate itself is read against the hound's 120 px/s advance and the belt's ~178 px depth: gentle enough to read as
// a drift rather than a strafe, and quick enough to finish inside the run-in its 600 px aggro range buys it.
DEFN_TEST(shipped_hound_is_the_only_unit_that_slides_along_the_belt) {
    UnitDataLoader catalog;
    DEFN_REQUIRE(catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS));

    const auto hound = catalog.get_unit("hound");
    DEFN_REQUIRE(hound.has_value());
    DEFN_CHECK_EQ(hound->belt_slide_speed_pixels_per_second, 40.0F);
    DEFN_CHECK(hound->belt_slide_speed_pixels_per_second < hound->move_speed_pixels_per_second);

    for (const char *unit_id : {"base", "breacher", "marksman", "impact", "operator", "grime", "mason", "wrecker", "jackal"}) {
        const auto other = catalog.get_unit(unit_id);
        DEFN_REQUIRE(other.has_value());
        DEFN_CHECK_EQ(other->belt_slide_speed_pixels_per_second, 0.0F);
    }
}

} // namespace defn
