// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef MATCH_SESSION_H
#define MATCH_SESSION_H

#include "match_summary.h"

#include <string>
#include <vector>

namespace defn {

struct MatchConfig {
    int starting_core_resource = 100;
    int initial_integrity = 3;
    double bounty_multiplier = 1.0;
    int energy_regen_rate = 1;
    // The reserve the player may hold, once they have spent down to it. Zero is uncapped, which is every authored
    // campaign level. See `MatchSession::apply_energy_ceiling` for why it engages rather than clamping from the
    // first tick.
    int energy_cap = 0;
    // The most friendlies that may stand on the belt at once. Zero is unlimited.
    //
    // This is the one rule that bounds what the player accumulates. Friendlies persist between waves and hostiles do
    // not, so without a cap the standing line integrates the whole difficulty ramp and settles at a fixed multiple
    // of any wave -- which is why no escalation rate alone can overrun it. See `ENDLESS_MODE.md`.
    int supply_cap = 0;
};

struct MatchRuntimeState {
    int core_resource = 100;
    int base_health = 300;
    int base_max_health = 300;
    int initial_integrity = 3;
    int enemies_killed = 0;
    int kill_score = 0;
    int survival_bonus = 0;
    int wave_reached = 0;
    int living_enemies = 0;
    int living_friendlies = 0;
    // Latched the first time the reserve falls to the cap, and never released.
    bool energy_ceiling_engaged = false;
    bool all_spawned = false;
    bool game_over = false;
};

class MatchSession {
  public:
    static constexpr int BASE_HEALTH_PER_HEART = 100;

    MatchSession() = default;

    void start(const MatchConfig &config);
    bool finish_game();

    bool is_game_over() const { return state_.game_over; }
    bool can_spend_energy(int amount) const;
    void spend_energy(int amount);
    void tick_energy();

    // Whether another friendly may take the field. Separate from affordability so a caller can tell the two apart:
    // a player who cannot afford a unit waits, and a player who is at the supply cap has to lose one first.
    [[nodiscard]] bool has_supply_room() const;
    void record_friendly_deployed();
    void record_friendly_died();

    // Raises or lowers the standing-line allowance mid-match. A mode whose difficulty compounds needs the line to
    // grow with it, or the only settings that keep the middle of a run tense are the ones that end it in eight
    // minutes -- measured, see `ENDLESS_MODE.md`. Never below what is already deployed: a cap that fell past the
    // living line would refuse deployments until the player was killed down to it, which reads as the game breaking
    // rather than as a rule.
    void set_supply_cap(int cap);
    void set_base_health(int current_health);

    // Income scaling applied on top of what the campaign upgrades already grant. A mode whose difficulty compounds
    // has to be able to lean on the economy without rebuilding it: the scale moves what a kill pays, and leaves the
    // score a kill is worth alone.
    void set_bounty_scale(double scale);

    // Points awarded for reaching a wave rather than for killing anything, so a run's length is legible in its score
    // instead of living on a second axis.
    void award_survival_bonus(int points);
    void record_wave_reached(int wave);

    void record_enemy_spawned();
    void mark_all_spawns_complete() { state_.all_spawned = true; }
    int record_enemy_died(int base_bounty);
    bool should_end_with_victory() const;

    int get_core_resource() const { return state_.core_resource; }
    int get_base_health() const { return state_.base_health; }
    int get_base_max_health() const { return state_.base_max_health; }
    int get_base_integrity() const;
    int get_initial_integrity() const { return state_.initial_integrity; }
    int get_enemies_killed() const { return state_.enemies_killed; }
    int get_kill_score() const { return state_.kill_score; }
    int get_living_enemies() const { return state_.living_enemies; }
    int get_living_friendlies() const { return state_.living_friendlies; }
    int get_supply_cap() const { return config_.supply_cap; }
    int get_energy_cap() const { return config_.energy_cap; }
    bool is_energy_ceiling_engaged() const { return state_.energy_ceiling_engaged; }

    int calculate_integrity_bonus() const;
    static int calculate_completion_bonus(bool victory);
    int calculate_level_score(bool victory) const;
    MatchSummaryModel build_end_game_summary(bool victory, int new_total_score, const std::string &current_level_id, const std::string &next_level_id,
                                             const std::vector<std::string> &new_unlocks) const;

  private:
    static int calculate_hearts_from_health(int health);
    // Adds to the reserve and returns what actually landed there, which is less than `amount` under the ceiling and
    // is what a caller should report to the player -- an energy pop for bounty the reserve could not hold would be
    // the readout lying about the rule.
    int credit_energy(int amount);
    void apply_energy_ceiling();

    MatchConfig config_{};
    // What the campaign's upgrades alone grant, so a mode scale is applied to that rather than compounding on itself.
    double base_bounty_multiplier_ = 1.0;
    // The largest line the level itself allows. A schedule may widen the allowance up to this and no further, so a
    // growth rate cannot quietly repeal the level's own rule.
    int supply_cap_ceiling_ = 0;
    MatchRuntimeState state_{};
};

} // namespace defn

#endif