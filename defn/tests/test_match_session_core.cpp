// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "match_session.h"

namespace defn {

DEFN_TEST(match_session_tracks_energy_score_and_victory_state) {
    MatchSession session;
    session.start({
        .starting_core_resource = 50,
        .initial_integrity = 3,
        .bounty_multiplier = 1.5F,
        .energy_regen_rate = 2,
    });

    DEFN_CHECK(session.can_spend_energy(50));
    session.spend_energy(20);
    DEFN_CHECK_EQ(session.get_core_resource(), 30);

    session.tick_energy();
    DEFN_CHECK_EQ(session.get_core_resource(), 32);

    session.record_enemy_spawned();
    const int awarded_bounty = session.record_enemy_died(11);
    session.mark_all_spawns_complete();
    session.set_base_health(200);

    DEFN_CHECK(session.should_end_with_victory());
    DEFN_CHECK_EQ(awarded_bounty, 17);
    DEFN_CHECK_EQ(session.get_core_resource(), 49);
    DEFN_CHECK_EQ(session.calculate_integrity_bonus(), 100);
    DEFN_CHECK_EQ(session.calculate_level_score(true), 211);
}

DEFN_TEST(match_session_energy_cap_engages_only_once_the_reserve_falls_to_it) {
    MatchSession session;
    // A starting grant above the cap is deliberate and is kept: the cap is a ratchet, not a clamp, so an opening
    // hand larger than the ceiling is still spendable.
    session.start({.starting_core_resource = 120, .initial_integrity = 3, .energy_regen_rate = 5, .energy_cap = 100});

    DEFN_CHECK(!session.is_energy_ceiling_engaged());
    DEFN_CHECK_EQ(session.get_core_resource(), 120);

    session.tick_energy();
    DEFN_CHECK_EQ(session.get_core_resource(), 125);
    DEFN_CHECK(!session.is_energy_ceiling_engaged());

    // Spending down to the cap latches it, and from there the ceiling holds against regeneration...
    session.spend_energy(30);
    DEFN_CHECK_EQ(session.get_core_resource(), 95);
    DEFN_CHECK(session.is_energy_ceiling_engaged());

    session.tick_energy();
    session.tick_energy();
    DEFN_CHECK_EQ(session.get_core_resource(), 100);

    // ...and against bounty, which is the half that decides a long run: income that can only ever refill the same
    // reserve stops compounding with the difficulty.
    session.record_enemy_spawned();
    const int awarded = session.record_enemy_died(40);
    DEFN_CHECK_EQ(session.get_core_resource(), 100);
    // What is reported is what actually landed, so a bounty pop cannot claim energy the reserve could not hold.
    DEFN_CHECK_EQ(awarded, 0);
}

DEFN_TEST(match_session_energy_cap_engages_at_once_when_the_opening_hand_is_under_it) {
    MatchSession session;
    session.start({.starting_core_resource = 60, .initial_integrity = 3, .energy_regen_rate = 100, .energy_cap = 100});

    DEFN_CHECK(session.is_energy_ceiling_engaged());
    session.tick_energy();
    DEFN_CHECK_EQ(session.get_core_resource(), 100);
}

DEFN_TEST(match_session_without_an_energy_cap_never_engages_a_ceiling) {
    MatchSession session;
    session.start({.starting_core_resource = 10, .initial_integrity = 3, .energy_regen_rate = 50});

    session.tick_energy();
    session.tick_energy();
    DEFN_CHECK(!session.is_energy_ceiling_engaged());
    DEFN_CHECK_EQ(session.get_core_resource(), 110);
}

DEFN_TEST(match_session_supply_cap_bounds_the_standing_line) {
    MatchSession session;
    session.start({.starting_core_resource = 100, .initial_integrity = 3, .supply_cap = 2});

    DEFN_CHECK(session.has_supply_room());
    session.record_friendly_deployed();
    session.record_friendly_deployed();
    DEFN_CHECK_EQ(session.get_living_friendlies(), 2);
    DEFN_CHECK(!session.has_supply_room());

    // A slot is freed by losing a body and by nothing else. This is the rule that bounds what the player
    // accumulates: friendlies persist between waves and hostiles do not, so an unbounded line integrates the whole
    // difficulty ramp -- see `ENDLESS_MODE.md`.
    session.record_friendly_died();
    DEFN_CHECK(session.has_supply_room());

    // The count never goes negative, so a death reported twice cannot mint a permanent extra slot.
    session.record_friendly_died();
    session.record_friendly_died();
    DEFN_CHECK_EQ(session.get_living_friendlies(), 0);
}

DEFN_TEST(match_session_supply_cap_widens_but_never_past_the_level_ceiling) {
    MatchSession session;
    session.start({.starting_core_resource = 100, .initial_integrity = 3, .supply_cap = 10});

    session.set_supply_cap(4);
    session.record_friendly_deployed();
    session.record_friendly_deployed();
    session.record_friendly_deployed();
    session.record_friendly_deployed();
    DEFN_CHECK(!session.has_supply_room());

    session.set_supply_cap(6);
    DEFN_CHECK(session.has_supply_room());

    // The level's own cap is the ceiling on what a schedule may open up, so a growth rate cannot repeal it.
    session.set_supply_cap(40);
    DEFN_CHECK_EQ(session.get_supply_cap(), 10);
}

DEFN_TEST(match_session_supply_cap_never_falls_below_the_standing_line) {
    MatchSession session;
    session.start({.starting_core_resource = 100, .initial_integrity = 3, .supply_cap = 12});
    for (int index = 0; index < 8; ++index) {
        session.record_friendly_deployed();
    }

    // A cap below what is already deployed would refuse every deployment until the player had been killed down to
    // it, which reads as the game breaking rather than as a rule.
    session.set_supply_cap(3);
    DEFN_CHECK_EQ(session.get_supply_cap(), 8);
}

DEFN_TEST(match_session_leaves_an_uncapped_match_uncapped_however_the_schedule_grows) {
    MatchSession session;
    session.start({.starting_core_resource = 100, .initial_integrity = 3});

    // Growth must not invent a limit a level deliberately did not set.
    session.set_supply_cap(5);
    for (int index = 0; index < 30; ++index) {
        session.record_friendly_deployed();
    }
    DEFN_CHECK(session.has_supply_room());
}

DEFN_TEST(match_session_without_a_supply_cap_always_has_room) {
    MatchSession session;
    session.start({.starting_core_resource = 100, .initial_integrity = 3});

    for (int index = 0; index < 50; ++index) {
        session.record_friendly_deployed();
    }
    DEFN_CHECK(session.has_supply_room());
}

DEFN_TEST(match_session_finish_game_is_idempotent) {
    MatchSession session;
    session.start({
        .starting_core_resource = 10,
        .initial_integrity = 1,
        .bounty_multiplier = 1.0F,
        .energy_regen_rate = 1,
    });

    DEFN_CHECK(session.finish_game());
    DEFN_CHECK(!session.finish_game());
    DEFN_CHECK(!session.can_spend_energy(1));
}

} // namespace defn