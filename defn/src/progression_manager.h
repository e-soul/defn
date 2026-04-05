#ifndef PROGRESSION_MANAGER_H
#define PROGRESSION_MANAGER_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <string>
#include <vector>

namespace defn {

using namespace godot;

struct UnitUnlock {
    String unit_id;
    int score_required = 0;
};

struct LevelUnlock {
    String level_id;
    int score_required = 0;
    String requires_completed; // empty = no prerequisite
};

struct UpgradeEntry {
    String id;
    int score_required = 0;
    String type;   // "starting_energy", "energy_regen", "bounty_mult"
    real_t value = 0.0;
};

class ProgressionManager : public Object {
    GDCLASS(ProgressionManager, Object)

  public:
    ProgressionManager() = default;

    static ProgressionManager *get_singleton();
    static void register_singleton();
    static void unregister_singleton();

    void initialize();

    // Queries
    int get_total_score() const { return total_score_; }
    PackedStringArray get_unlocked_units() const;
    PackedStringArray get_unlocked_levels() const;
    bool is_level_completed(const String &level_id) const;
    bool is_level_unlocked(const String &level_id) const;
    int get_highest_level_score(const String &level_id) const;
    int get_effective_starting_energy(int base) const;
    int get_effective_energy_regen() const;
    real_t get_effective_bounty_multiplier() const;

    String get_current_level_id() const { return current_level_id_; }
    void set_current_level_id(const String &level_id) { current_level_id_ = level_id; }

    // Score screen helpers
    int get_score_required_for_level(const String &level_id) const;
    PackedStringArray compute_new_unlocks(int old_score, int new_score) const;

    // Level unlock data for UI
    const std::vector<LevelUnlock> &get_level_unlock_data() const { return level_unlocks_; }

    // Mutators
    void add_score(int amount);
    void mark_level_completed(const String &level_id, int level_score);
    void save();

  protected:
    static void _bind_methods();

  private:
    void load_progression_data();
    void load_save();
    void create_default_save();

    static ProgressionManager *singleton_;

    // Progression data (from progression.json)
    std::vector<UnitUnlock> unit_unlocks_;
    std::vector<LevelUnlock> level_unlocks_;
    std::vector<UpgradeEntry> upgrades_;

    // Save state
    int total_score_ = 0;
    std::vector<String> levels_completed_;
    std::vector<std::pair<String, int>> highest_level_scores_;

    // Transient
    String current_level_id_ = "level_01";
};

} // namespace defn

#endif
