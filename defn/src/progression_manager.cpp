#include "progression_manager.h"
#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

constexpr auto PROGRESSION_PATH = "res://data/progression.json";
constexpr auto SAVE_PATH = "user://save_data.json";

} // namespace

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
    catalog_.load(PROGRESSION_PATH);
    load_save();
}

void ProgressionManager::load_save() {
    const auto loaded_save = ProgressionSaveRepository::load(SAVE_PATH);
    if (!loaded_save) {
        UtilityFunctions::print("ProgressionManager: No save file found, creating default");
        create_default_save();
        return;
    }

    save_data_ = *loaded_save;
    UtilityFunctions::print("ProgressionManager: Loaded save — total_score=", save_data_.total_score,
                            ", levels_completed=", save_data_.levels_completed.size());
}

void ProgressionManager::create_default_save() {
    save_data_ = {};
    save();
}

void ProgressionManager::save() {
    if (!ProgressionSaveRepository::save(SAVE_PATH, save_data_)) {
        return;
    }

    UtilityFunctions::print("ProgressionManager: Save written — total_score=", save_data_.total_score);
}

PackedStringArray ProgressionManager::get_unlocked_units() const {
    PackedStringArray result;
    for (const auto &unlock : catalog_.get_unit_unlocks()) {
        if (save_data_.total_score >= unlock.score_required) {
            result.push_back(unlock.unit_id);
        }
    }
    return result;
}

PackedStringArray ProgressionManager::get_unlocked_levels() const {
    PackedStringArray result;
    for (const auto &unlock : catalog_.get_level_unlocks()) {
        if (is_level_unlocked(unlock.level_id)) {
            result.push_back(unlock.level_id);
        }
    }
    return result;
}

bool ProgressionManager::is_level_completed(const String &level_id) const {
    return std::ranges::find(save_data_.levels_completed, level_id) != save_data_.levels_completed.end();
}

bool ProgressionManager::is_level_unlocked(const String &level_id) const {
    for (const auto &unlock : catalog_.get_level_unlocks()) {
        if (unlock.level_id == level_id) {
            if (save_data_.total_score < unlock.score_required) {
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
    for (const auto &[level, score] : save_data_.highest_level_scores) {
        if (level == level_id) {
            return score;
        }
    }
    return 0;
}

int ProgressionManager::get_effective_starting_energy(int base) const {
    int bonus = 0;
    for (const auto &upgrade : catalog_.get_upgrades()) {
        if (upgrade.type == "starting_energy" && save_data_.total_score >= upgrade.score_required) {
            bonus += static_cast<int>(upgrade.value);
        }
    }
    return base + bonus;
}

int ProgressionManager::get_effective_energy_regen() const {
    int regen = 1; // base regen
    for (const auto &upgrade : catalog_.get_upgrades()) {
        if (upgrade.type == "energy_regen" && save_data_.total_score >= upgrade.score_required) {
            int val = static_cast<int>(upgrade.value);
            regen = std::max(val, regen);
        }
    }
    return regen;
}

real_t ProgressionManager::get_effective_bounty_multiplier() const {
    real_t mult = 1.0;
    for (const auto &upgrade : catalog_.get_upgrades()) {
        if (upgrade.type == "bounty_mult" && save_data_.total_score >= upgrade.score_required) {
            mult = std::max(upgrade.value, mult);
        }
    }
    return mult;
}

int ProgressionManager::get_score_required_for_level(const String &level_id) const {
    for (const auto &unlock : catalog_.get_level_unlocks()) {
        if (unlock.level_id == level_id) {
            return unlock.score_required;
        }
    }
    return 0;
}

void ProgressionManager::add_score(int amount) { save_data_.total_score += amount; }

void ProgressionManager::mark_level_completed(const String &level_id, int level_score) {
    if (!is_level_completed(level_id)) {
        save_data_.levels_completed.push_back(level_id);
    }

    // Update highest score
    bool found = false;
    for (auto &[level, score] : save_data_.highest_level_scores) {
        if (level == level_id) {
            score = std::max(level_score, score);
            found = true;
            break;
        }
    }
    if (!found) {
        save_data_.highest_level_scores.emplace_back(level_id, level_score);
    }
}

} // namespace defn
