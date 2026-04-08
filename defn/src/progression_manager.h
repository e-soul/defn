#ifndef PROGRESSION_MANAGER_H
#define PROGRESSION_MANAGER_H

#include "progression_catalog.h"
#include "progression_save_repository.h"
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

class ProgressionManager : public Object {
    GDCLASS(ProgressionManager, Object)

  public:
    ProgressionManager() = default;

    static ProgressionManager *get_singleton();
    static void register_singleton();
    static void unregister_singleton();

    void initialize();

    // Queries
    int get_total_score() const { return save_data_.total_score; }
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

    // Progression queries for gameplay and presentation
    int get_score_required_for_level(const String &level_id) const;
    const std::vector<UnitUnlock> &get_unit_unlock_data() const { return catalog_.get_unit_unlocks(); }
    const std::vector<LevelUnlock> &get_level_unlock_data() const { return catalog_.get_level_unlocks(); }
    const std::vector<UpgradeEntry> &get_upgrade_data() const { return catalog_.get_upgrades(); }

    // Mutators
    void add_score(int amount);
    void mark_level_completed(const String &level_id, int level_score);
    void save();

  protected:
    static void _bind_methods();

  private:
    void load_save();
    void create_default_save();

    static ProgressionManager *singleton_;

    ProgressionCatalog catalog_;
    ProgressionSaveData save_data_;

    // Transient
    String current_level_id_ = "level_01";
};

} // namespace defn

#endif
