#include "progression_manager.h"
#include "variant_tools.h"
#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

ProgressionManager *ProgressionManager::singleton_ = nullptr;

void ProgressionManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_total_score"), &ProgressionManager::get_total_score);
    ClassDB::bind_method(D_METHOD("get_unlocked_units"), &ProgressionManager::get_unlocked_units);
    ClassDB::bind_method(D_METHOD("get_unlocked_levels"), &ProgressionManager::get_unlocked_levels);
    ClassDB::bind_method(D_METHOD("is_level_completed", "level_id"), &ProgressionManager::is_level_completed);
    ClassDB::bind_method(D_METHOD("is_level_unlocked", "level_id"), &ProgressionManager::is_level_unlocked);
    ClassDB::bind_method(D_METHOD("get_current_level_id"), &ProgressionManager::get_current_level_id);
    ClassDB::bind_method(D_METHOD("set_current_level_id", "level_id"), &ProgressionManager::set_current_level_id);
    ClassDB::bind_method(D_METHOD("add_score", "amount"), &ProgressionManager::add_score);
    ClassDB::bind_method(D_METHOD("save"), &ProgressionManager::save);
}

ProgressionManager *ProgressionManager::get_singleton() { return singleton_; }

void ProgressionManager::register_singleton() {
    if (singleton_ != nullptr) {
        return;
    }
    singleton_ = memnew(ProgressionManager);
    Engine::get_singleton()->register_singleton("ProgressionManager", singleton_);
    singleton_->initialize();
}

void ProgressionManager::unregister_singleton() {
    if (singleton_ == nullptr) {
        return;
    }
    Engine::get_singleton()->unregister_singleton("ProgressionManager");
    memdelete(singleton_);
    singleton_ = nullptr;
}

void ProgressionManager::initialize() {
    load_progression_data();
    load_save();
}

void ProgressionManager::load_progression_data() {
    const String path = "res://data/progression.json";
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (!file.is_valid()) {
        UtilityFunctions::printerr("ProgressionManager: Failed to open: ", path);
        return;
    }

    String json_text = file->get_as_text();
    file->close();

    Ref<JSON> json;
    json.instantiate();
    if (json->parse(json_text) != OK) {
        UtilityFunctions::printerr("ProgressionManager: JSON parse error: ", json->get_error_message());
        return;
    }

    Dictionary data = json->get_data();

    // Unit unlocks
    Array unit_arr = data.get("unit_unlocks", Array());
    unit_unlocks_.clear();
    for (const auto &entry_var : unit_arr) {
        Dictionary entry = entry_var;
        UnitUnlock unlock;
        unlock.unit_id = String(entry.get("unit_id", ""));
        unlock.score_required = VariantTools::as_int(entry.get("score_required", 0));
        unit_unlocks_.push_back(unlock);
    }

    // Level unlocks
    Array level_arr = data.get("level_unlocks", Array());
    level_unlocks_.clear();
    for (const auto &entry_var : level_arr) {
        Dictionary entry = entry_var;
        LevelUnlock unlock;
        unlock.level_id = String(entry.get("level_id", ""));
        unlock.score_required = VariantTools::as_int(entry.get("score_required", 0));
        Variant req = entry.get("requires_completed", Variant());
        if (req.get_type() == Variant::STRING) {
            unlock.requires_completed = String(req);
        }
        level_unlocks_.push_back(unlock);
    }

    // Upgrades
    Array upgrade_arr = data.get("upgrades", Array());
    upgrades_.clear();
    for (const auto &entry_var : upgrade_arr) {
        Dictionary entry = entry_var;
        UpgradeEntry upgrade;
        upgrade.id = String(entry.get("id", ""));
        upgrade.score_required = VariantTools::as_int(entry.get("score_required", 0));
        upgrade.type = String(entry.get("type", ""));
        upgrade.value = VariantTools::as_real(entry.get("value", 0.0));
        upgrades_.push_back(upgrade);
    }

    UtilityFunctions::print("ProgressionManager: Loaded ", unit_unlocks_.size(), " unit unlocks, ", level_unlocks_.size(), " level unlocks, ", upgrades_.size(),
                            " upgrades");
}

void ProgressionManager::load_save() {
    const String path = "user://save_data.json";
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (!file.is_valid()) {
        UtilityFunctions::print("ProgressionManager: No save file found, creating default");
        create_default_save();
        return;
    }

    String json_text = file->get_as_text();
    file->close();

    Ref<JSON> json;
    json.instantiate();
    if (json->parse(json_text) != OK) {
        UtilityFunctions::printerr("ProgressionManager: Save file corrupted, creating default");
        create_default_save();
        return;
    }

    Dictionary data = json->get_data();
    total_score_ = VariantTools::as_int(data.get("total_score", 0));

    Array completed = data.get("levels_completed", Array());
    levels_completed_.clear();
    for (const auto &level_var : completed) {
        levels_completed_.push_back(String(level_var));
    }

    Dictionary highest = data.get("highest_level_score", Dictionary());
    highest_level_scores_.clear();
    Array keys = highest.keys();
    for (const auto &key_var : keys) {
        String key = key_var;
        int score = VariantTools::as_int(highest[key]);
        highest_level_scores_.emplace_back(key, score);
    }

    UtilityFunctions::print("ProgressionManager: Loaded save — total_score=", total_score_, ", levels_completed=", completed.size());
}

void ProgressionManager::create_default_save() {
    total_score_ = 0;
    levels_completed_.clear();
    highest_level_scores_.clear();
    save();
}

void ProgressionManager::save() {
    Dictionary data;
    data["version"] = 1;
    data["total_score"] = total_score_;

    Array completed;
    for (const auto &level_id : levels_completed_) {
        completed.push_back(level_id);
    }
    data["levels_completed"] = completed;

    Dictionary highest;
    for (const auto &[level_id, score] : highest_level_scores_) {
        highest[level_id] = score;
    }
    data["highest_level_score"] = highest;

    String json_text = JSON::stringify(data, "  ");

    Ref<FileAccess> file = FileAccess::open("user://save_data.json", FileAccess::WRITE);
    if (!file.is_valid()) {
        UtilityFunctions::printerr("ProgressionManager: Failed to write save file");
        return;
    }
    file->store_string(json_text);
    file->close();

    UtilityFunctions::print("ProgressionManager: Save written — total_score=", total_score_);
}

PackedStringArray ProgressionManager::get_unlocked_units() const {
    PackedStringArray result;
    for (const auto &unlock : unit_unlocks_) {
        if (total_score_ >= unlock.score_required) {
            result.push_back(unlock.unit_id);
        }
    }
    return result;
}

PackedStringArray ProgressionManager::get_unlocked_levels() const {
    PackedStringArray result;
    for (const auto &unlock : level_unlocks_) {
        if (is_level_unlocked(unlock.level_id)) {
            result.push_back(unlock.level_id);
        }
    }
    return result;
}

bool ProgressionManager::is_level_completed(const String &level_id) const { return std::ranges::find(levels_completed_, level_id) != levels_completed_.end(); }

bool ProgressionManager::is_level_unlocked(const String &level_id) const {
    for (const auto &unlock : level_unlocks_) {
        if (unlock.level_id == level_id) {
            if (total_score_ < unlock.score_required) {
                return false;
            }
            if (!unlock.requires_completed.is_empty() && !is_level_completed(unlock.requires_completed)) {
                return false;
            }
            return true;
        }
    }
    return false;
}

int ProgressionManager::get_highest_level_score(const String &level_id) const {
    for (const auto &[level, score] : highest_level_scores_) {
        if (level == level_id) {
            return score;
        }
    }
    return 0;
}

int ProgressionManager::get_effective_starting_energy(int base) const {
    int bonus = 0;
    for (const auto &upgrade : upgrades_) {
        if (upgrade.type == "starting_energy" && total_score_ >= upgrade.score_required) {
            bonus += static_cast<int>(upgrade.value);
        }
    }
    return base + bonus;
}

int ProgressionManager::get_effective_energy_regen() const {
    int regen = 1; // base regen
    for (const auto &upgrade : upgrades_) {
        if (upgrade.type == "energy_regen" && total_score_ >= upgrade.score_required) {
            int val = static_cast<int>(upgrade.value);
            regen = std::max(val, regen);
        }
    }
    return regen;
}

real_t ProgressionManager::get_effective_bounty_multiplier() const {
    real_t mult = 1.0;
    for (const auto &upgrade : upgrades_) {
        if (upgrade.type == "bounty_mult" && total_score_ >= upgrade.score_required) {
            mult = std::max(upgrade.value, mult);
        }
    }
    return mult;
}

int ProgressionManager::get_score_required_for_level(const String &level_id) const {
    for (const auto &unlock : level_unlocks_) {
        if (unlock.level_id == level_id) {
            return unlock.score_required;
        }
    }
    return 0;
}

PackedStringArray ProgressionManager::compute_new_unlocks(int old_score, int new_score) const {
    PackedStringArray result;

    for (const auto &unlock : unit_unlocks_) {
        if (old_score < unlock.score_required && new_score >= unlock.score_required) {
            result.push_back(vformat("NEW UNLOCK: %s unit!", unlock.unit_id.capitalize()));
        }
    }

    for (const auto &unlock : level_unlocks_) {
        if (old_score < unlock.score_required && new_score >= unlock.score_required) {
            result.push_back(vformat("NEW UNLOCK: %s!", unlock.level_id.replace("_", " ").capitalize()));
        }
    }

    for (const auto &upgrade : upgrades_) {
        if (old_score < upgrade.score_required && new_score >= upgrade.score_required) {
            if (upgrade.type == "starting_energy") {
                result.push_back(vformat("NEW UPGRADE: +%d starting energy!", static_cast<int>(upgrade.value)));
            } else if (upgrade.type == "energy_regen") {
                result.push_back(vformat("NEW UPGRADE: Energy regen %d/sec!", static_cast<int>(upgrade.value)));
            } else if (upgrade.type == "bounty_mult") {
                result.push_back(vformat(String::utf8("NEW UPGRADE: \u00D7%.1f bounty from kills!"), upgrade.value));
            }
        }
    }

    return result;
}

void ProgressionManager::add_score(int amount) { total_score_ += amount; }

void ProgressionManager::mark_level_completed(const String &level_id, int level_score) {
    if (!is_level_completed(level_id)) {
        levels_completed_.push_back(level_id);
    }

    // Update highest score
    bool found = false;
    for (auto &[level, score] : highest_level_scores_) {
        if (level == level_id) {
            score = std::max(level_score, score);
            found = true;
            break;
        }
    }
    if (!found) {
        highest_level_scores_.emplace_back(level_id, level_score);
    }
}

} // namespace defn
