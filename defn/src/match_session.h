#ifndef MATCH_SESSION_H
#define MATCH_SESSION_H

#include "score_screen_models.h"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace defn {

using namespace godot;

struct MatchConfig {
    int starting_core_resource = 100;
    int initial_integrity = 3;
    real_t bounty_multiplier = 1.0F;
    int energy_regen_rate = 1;
};

struct MatchRuntimeState {
    int core_resource = 100;
    int base_health = 300;
    int base_max_health = 300;
    int initial_integrity = 3;
    int enemies_killed = 0;
    int kill_score = 0;
    int living_enemies = 0;
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
    void set_base_health(int current_health);

    void record_enemy_spawned();
    void mark_all_spawns_complete() { state_.all_spawned = true; }
    void record_enemy_died(int base_bounty);
    bool should_end_with_victory() const;

    int get_core_resource() const { return state_.core_resource; }
    int get_base_health() const { return state_.base_health; }
    int get_base_max_health() const { return state_.base_max_health; }
    int get_base_integrity() const;
    int get_initial_integrity() const { return state_.initial_integrity; }
    int get_enemies_killed() const { return state_.enemies_killed; }
    int get_kill_score() const { return state_.kill_score; }
    int get_living_enemies() const { return state_.living_enemies; }

    int calculate_integrity_bonus() const;
    static int calculate_completion_bonus(bool victory);
    int calculate_level_score(bool victory) const;
    ScoreScreenModel build_end_game_summary(bool victory, int new_total_score, const String &current_level_id, const String &next_level_id,
                                            const PackedStringArray &new_unlocks, const ScoreScreenRewardModel &reward) const;

  private:
    static int calculate_hearts_from_health(int health);

    MatchConfig config_{};
    MatchRuntimeState state_{};
};

} // namespace defn

#endif