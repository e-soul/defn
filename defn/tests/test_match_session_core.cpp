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