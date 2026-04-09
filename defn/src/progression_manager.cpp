#include "progression_manager.h"

#include <algorithm>
#include <array>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <random>

namespace defn {

namespace {

constexpr auto PROGRESSION_PATH = "res://data/progression.json";
constexpr auto UPGRADE_PATH = "res://data/upgrades.json";
constexpr auto SAVE_PATH = "user://save_data.json";
constexpr int CURRENT_SAVE_VERSION = 2;
constexpr auto LEGACY_REWARD_SENTINEL = "__legacy_reward_claimed__";

struct LegacyMigrationRule {
    int score_required = 0;
    const char *upgrade_id = "";
};

constexpr std::array<LegacyMigrationRule, 8> LEGACY_MIGRATION_RULES{{
    {.score_required = 50, .upgrade_id = "battery_pack"},
    {.score_required = 100, .upgrade_id = "sharpshooter_contract"},
    {.score_required = 300, .upgrade_id = "overclocked_capacitors"},
    {.score_required = 500, .upgrade_id = "demolition_permit"},
    {.score_required = 800, .upgrade_id = "salvage_rights"},
    {.score_required = 1200, .upgrade_id = "reserve_cells"},
    {.score_required = 1500, .upgrade_id = "command_uplink"},
    {.score_required = 2000, .upgrade_id = "war_chest"},
}};

void push_unique(PackedStringArray &array, const String &value) {
    if (!array.has(value)) {
        array.push_back(value);
    }
}

int choose_weighted_index(const std::vector<const UpgradeCardDefinition *> &candidates, std::mt19937 &rng) {
    int total_weight = 0;
    for (const UpgradeCardDefinition *candidate : candidates) {
        total_weight += std::max(candidate->weight, 1);
    }

    std::uniform_int_distribution<int> distribution(1, total_weight);
    int roll = distribution(rng);
    for (int index = 0; std::cmp_less(index, candidates.size()); ++index) {
        roll -= std::max(candidates[static_cast<size_t>(index)]->weight, 1);
        if (roll <= 0) {
            return index;
        }
    }

    return static_cast<int>(candidates.size()) - 1;
}

const UpgradeCardDefinition *pick_and_remove(std::vector<const UpgradeCardDefinition *> &candidates, std::mt19937 &rng) {
    if (candidates.empty()) {
        return nullptr;
    }

    const int index = choose_weighted_index(candidates, rng);
    const UpgradeCardDefinition *choice = candidates[static_cast<size_t>(index)];
    candidates.erase(candidates.begin() + index);
    return choice;
}

} // namespace

ProgressionManager *ProgressionManager::singleton_ = nullptr;

void ProgressionManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_total_score"), &ProgressionManager::get_total_score);
    ClassDB::bind_method(D_METHOD("get_unlocked_units"), &ProgressionManager::get_unlocked_units);
    ClassDB::bind_method(D_METHOD("get_unlocked_levels"), &ProgressionManager::get_unlocked_levels);
    ClassDB::bind_method(D_METHOD("can_claim_level_upgrade", "level_id"), &ProgressionManager::can_claim_level_upgrade);
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
    upgrade_catalog_.load(UPGRADE_PATH);
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
    migrate_legacy_save_if_needed();
    UtilityFunctions::print("ProgressionManager: Loaded save — total_score=", save_data_.total_score, ", levels_completed=", save_data_.levels_completed.size(),
                            ", owned_upgrades=", save_data_.owned_upgrades.size());
}

void ProgressionManager::create_default_save() {
    save_data_ = {};
    save_data_.schema_version = CURRENT_SAVE_VERSION;
    save();
}

void ProgressionManager::save() {
    save_data_.schema_version = CURRENT_SAVE_VERSION;
    if (!ProgressionSaveRepository::save(SAVE_PATH, save_data_)) {
        return;
    }

    UtilityFunctions::print("ProgressionManager: Save written — total_score=", save_data_.total_score);
}

PackedStringArray ProgressionManager::get_unlocked_units() const {
    PackedStringArray result;
    for (const auto &unit_id : upgrade_catalog_.get_base_units()) {
        push_unique(result, unit_id);
    }

    for (const auto &owned_upgrade : save_data_.owned_upgrades) {
        const UpgradeCardDefinition *card = upgrade_catalog_.find_card(owned_upgrade);
        if (card == nullptr) {
            continue;
        }

        for (const auto &effect : card->effects) {
            if (effect.type == UpgradeEffectType::UNIT_UNLOCK && !effect.unit_id.is_empty()) {
                push_unique(result, effect.unit_id);
            }
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

PackedStringArray ProgressionManager::get_owned_upgrades() const {
    PackedStringArray result;
    for (const auto &upgrade_id : save_data_.owned_upgrades) {
        result.push_back(upgrade_id);
    }
    return result;
}

bool ProgressionManager::has_upgrade(const String &upgrade_id) const { return get_owned_upgrade_count(upgrade_id) > 0; }

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

bool ProgressionManager::can_claim_level_upgrade(const String &level_id) const { return get_claimed_upgrade_for_level(level_id).is_empty(); }

String ProgressionManager::get_claimed_upgrade_for_level(const String &level_id) const {
    for (const auto &[claimed_level_id, claimed_upgrade_id] : save_data_.claimed_level_upgrades) {
        if (claimed_level_id == level_id) {
            return claimed_upgrade_id;
        }
    }
    return {};
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

    for (const auto &owned_upgrade : save_data_.owned_upgrades) {
        const UpgradeCardDefinition *card = upgrade_catalog_.find_card(owned_upgrade);
        if (card == nullptr) {
            continue;
        }

        for (const auto &effect : card->effects) {
            if (effect.type == UpgradeEffectType::STARTING_ENERGY_DELTA) {
                bonus += static_cast<int>(effect.value);
            }
        }
    }

    return base + bonus;
}

int ProgressionManager::get_effective_energy_regen() const {
    int regen = 1; // base regen

    for (const auto &owned_upgrade : save_data_.owned_upgrades) {
        const UpgradeCardDefinition *card = upgrade_catalog_.find_card(owned_upgrade);
        if (card == nullptr) {
            continue;
        }

        for (const auto &effect : card->effects) {
            if (effect.type == UpgradeEffectType::ENERGY_REGEN_DELTA) {
                regen += static_cast<int>(effect.value);
            }
        }
    }

    return regen;
}

real_t ProgressionManager::get_effective_bounty_multiplier() const {
    real_t mult = 1.0;

    for (const auto &owned_upgrade : save_data_.owned_upgrades) {
        const UpgradeCardDefinition *card = upgrade_catalog_.find_card(owned_upgrade);
        if (card == nullptr) {
            continue;
        }

        for (const auto &effect : card->effects) {
            if (effect.type == UpgradeEffectType::BOUNTY_MULTIPLIER_DELTA) {
                mult += effect.value;
            }
        }
    }

    return mult;
}

int ProgressionManager::get_effective_base_integrity(int base) const {
    int integrity_bonus = 0;

    for (const auto &owned_upgrade : save_data_.owned_upgrades) {
        const UpgradeCardDefinition *card = upgrade_catalog_.find_card(owned_upgrade);
        if (card == nullptr) {
            continue;
        }

        for (const auto &effect : card->effects) {
            if (effect.type == UpgradeEffectType::BASE_INTEGRITY_DELTA) {
                integrity_bonus += static_cast<int>(effect.value);
            }
        }
    }

    return base + integrity_bonus;
}

int ProgressionManager::get_completed_level_count() const { return static_cast<int>(save_data_.levels_completed.size()); }

int ProgressionManager::get_score_required_for_level(const String &level_id) const {
    for (const auto &unlock : catalog_.get_level_unlocks()) {
        if (unlock.level_id == level_id) {
            return unlock.score_required;
        }
    }
    return 0;
}

Array ProgressionManager::build_upgrade_draft_for_level(const String &level_id) const {
    Array result;
    if (!can_claim_level_upgrade(level_id)) {
        return result;
    }

    std::vector<const UpgradeCardDefinition *> unit_candidates;
    std::vector<const UpgradeCardDefinition *> general_candidates;
    const int completed_levels = get_completed_level_count();

    for (const auto &card : upgrade_catalog_.get_cards()) {
        if (get_owned_upgrade_count(card.id) >= card.max_picks) {
            continue;
        }
        if (completed_levels < card.minimum_completed_levels) {
            continue;
        }

        bool prerequisites_met = true;
        for (const auto &prerequisite : card.prerequisites) {
            if (!has_upgrade(prerequisite)) {
                prerequisites_met = false;
                break;
            }
        }
        if (!prerequisites_met) {
            continue;
        }

        if (card.grants_unit_unlock()) {
            unit_candidates.push_back(&card);
        } else {
            general_candidates.push_back(&card);
        }
    }

    std::random_device random_device;
    std::mt19937 rng(random_device());
    std::vector<const UpgradeCardDefinition *> selected_cards;
    const int draft_size = std::max(1, upgrade_catalog_.get_draft_size());

    if (upgrade_catalog_.should_reserve_unit_slot()) {
        if (const UpgradeCardDefinition *unit_pick = pick_and_remove(unit_candidates, rng); unit_pick != nullptr) {
            selected_cards.push_back(unit_pick);
        }
    }

    std::vector<const UpgradeCardDefinition *> remaining_candidates = general_candidates;
    remaining_candidates.insert(remaining_candidates.end(), unit_candidates.begin(), unit_candidates.end());

    while (static_cast<int>(selected_cards.size()) < draft_size) {
        const UpgradeCardDefinition *pick = pick_and_remove(remaining_candidates, rng);
        if (pick == nullptr) {
            break;
        }
        selected_cards.push_back(pick);
    }

    for (const UpgradeCardDefinition *card : selected_cards) {
        result.push_back(build_upgrade_card_view(*card));
    }

    return result;
}

Dictionary ProgressionManager::get_upgrade_card_view(const String &upgrade_id) const {
    const UpgradeCardDefinition *card = upgrade_catalog_.find_card(upgrade_id);
    return card == nullptr ? Dictionary() : build_upgrade_card_view(*card);
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

bool ProgressionManager::claim_level_upgrade(const String &level_id, const String &upgrade_id) {
    if (level_id.is_empty() || upgrade_id.is_empty() || !can_claim_level_upgrade(level_id)) {
        return false;
    }

    const UpgradeCardDefinition *card = upgrade_catalog_.find_card(upgrade_id);
    if (card == nullptr || get_owned_upgrade_count(upgrade_id) >= card->max_picks) {
        return false;
    }

    save_data_.claimed_level_upgrades.emplace_back(level_id, upgrade_id);
    grant_upgrade(upgrade_id);
    return true;
}

Dictionary ProgressionManager::build_upgrade_card_view(const UpgradeCardDefinition &card) {
    Dictionary card_view;
    card_view["id"] = card.id;
    card_view["name"] = card.name;
    card_view["description"] = card.description;
    card_view["emoji"] = card.emoji;
    card_view["category"] = card.category;
    return card_view;
}

void ProgressionManager::migrate_legacy_save_if_needed() {
    if (save_data_.schema_version >= CURRENT_SAVE_VERSION) {
        return;
    }

    for (const auto &level_id : save_data_.levels_completed) {
        if (get_claimed_upgrade_for_level(level_id).is_empty()) {
            save_data_.claimed_level_upgrades.emplace_back(level_id, String(LEGACY_REWARD_SENTINEL));
        }
    }

    for (const auto &rule : LEGACY_MIGRATION_RULES) {
        if (save_data_.total_score >= rule.score_required) {
            grant_upgrade(String(rule.upgrade_id));
        }
    }

    save_data_.schema_version = CURRENT_SAVE_VERSION;
    save();
}

void ProgressionManager::grant_upgrade(const String &upgrade_id) {
    const UpgradeCardDefinition *card = upgrade_catalog_.find_card(upgrade_id);
    if (card == nullptr || get_owned_upgrade_count(upgrade_id) >= card->max_picks) {
        return;
    }

    save_data_.owned_upgrades.push_back(upgrade_id);
}

int ProgressionManager::get_owned_upgrade_count(const String &upgrade_id) const {
    return static_cast<int>(std::ranges::count(save_data_.owned_upgrades, upgrade_id));
}

} // namespace defn
