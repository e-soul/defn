#include "progression_manager.h"

#include "content_validator.h"
#include "data_paths.h"
#include "unit_data.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <random>

namespace defn {

namespace {

int round_effect_delta(real_t value) { return static_cast<int>(std::lround(static_cast<double>(value))); }

template <typename Func> void for_each_owned_upgrade_effect(const PlayerProfile &profile, const UpgradeCatalog &catalog, Func &&func) {
    for (const auto &[owned_upgrade, count] : profile.owned_upgrade_counts) {
        const UpgradeCardDefinition *card = catalog.find_card(owned_upgrade);
        if (card == nullptr || count <= 0) {
            continue;
        }

        for (int pick = 0; pick < count; ++pick) {
            for (const auto &effect : card->effects) {
                func(*card, effect);
            }
        }
    }
}

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

CampaignService *CampaignService::singleton_ = nullptr;

void CampaignService::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_total_score"), &CampaignService::get_total_score);
    ClassDB::bind_method(D_METHOD("get_unlocked_units"), &CampaignService::get_unlocked_units);
    ClassDB::bind_method(D_METHOD("get_unlocked_levels"), &CampaignService::get_unlocked_levels);
    ClassDB::bind_method(D_METHOD("can_claim_level_upgrade", "level_id"), &CampaignService::can_claim_level_upgrade);
    ClassDB::bind_method(D_METHOD("can_claim_rescue_draft", "level_id"), &CampaignService::can_claim_rescue_draft);
    ClassDB::bind_method(D_METHOD("is_level_completed", "level_id"), &CampaignService::is_level_completed);
    ClassDB::bind_method(D_METHOD("is_level_unlocked", "level_id"), &CampaignService::is_level_unlocked);
    ClassDB::bind_method(D_METHOD("get_frontier_level_id"), &CampaignService::get_frontier_level_id);
    ClassDB::bind_method(D_METHOD("get_current_level_id"), &CampaignService::get_current_level_id);
    ClassDB::bind_method(D_METHOD("set_current_level_id", "level_id"), &CampaignService::set_current_level_id);
    ClassDB::bind_method(D_METHOD("add_score", "amount"), &CampaignService::add_score);
    ClassDB::bind_method(D_METHOD("save"), &CampaignService::save);
}

CampaignService *CampaignService::get_singleton() { return singleton_; }

void CampaignService::register_singleton() {
    if (singleton_ != nullptr) {
        return;
    }
    singleton_ = memnew(CampaignService);
    Engine::get_singleton()->register_singleton("CampaignService", singleton_);
    singleton_->initialize();
}

void CampaignService::unregister_singleton() {
    if (singleton_ == nullptr) {
        return;
    }
    Engine::get_singleton()->unregister_singleton("CampaignService");
    memdelete(singleton_);
    singleton_ = nullptr;
}

void CampaignService::initialize() {
    ContentValidator::report_startup_validation();
    catalog_.load(DataPaths::PROGRESSION_DATA);
    upgrade_catalog_.load(DataPaths::UPGRADES_DATA);
    load_save();
}

void CampaignService::load_save() {
    const auto loaded_save = ProgressionSaveRepository::load(DataPaths::SAVE_DATA);
    if (!loaded_save) {
        UtilityFunctions::print("CampaignService: No save file found, creating default");
        create_default_save();
        return;
    }

    save_data_ = *loaded_save;
    UtilityFunctions::print("CampaignService: Loaded save — total_score=", save_data_.total_score, ", completed_levels=", save_data_.completed_levels.size(),
                            ", owned_upgrades=", save_data_.owned_upgrade_counts.size());
}

void CampaignService::create_default_save() {
    save_data_ = {};
    save();
}

void CampaignService::save() {
    if (!ProgressionSaveRepository::save(DataPaths::SAVE_DATA, save_data_)) {
        return;
    }

    UtilityFunctions::print("CampaignService: Save written — total_score=", save_data_.total_score);
}

PackedStringArray CampaignService::get_unlocked_units() const {
    PackedStringArray result;
    for (const auto &unit_id : upgrade_catalog_.get_base_units()) {
        push_unique(result, unit_id);
    }

    for_each_owned_upgrade_effect(save_data_, upgrade_catalog_, [&result](const UpgradeCardDefinition &, const UpgradeCardEffect &effect) {
        if (effect.type == UpgradeEffectType::UNIT_UNLOCK && !effect.unit_id.is_empty()) {
            push_unique(result, effect.unit_id);
        }
    });
    return result;
}

PackedStringArray CampaignService::get_unlocked_levels() const {
    PackedStringArray result;
    for (const auto &unlock : catalog_.get_level_unlocks()) {
        if (is_level_unlocked(unlock.level_id)) {
            result.push_back(unlock.level_id);
        }
    }
    return result;
}

PackedStringArray CampaignService::get_owned_upgrades() const {
    PackedStringArray result;
    for (const auto &[upgrade_id, count] : save_data_.owned_upgrade_counts) {
        for (int pick = 0; pick < count; ++pick) {
            result.push_back(upgrade_id);
        }
    }
    return result;
}

bool CampaignService::has_upgrade(const String &upgrade_id) const { return get_owned_upgrade_count(upgrade_id) > 0; }

bool CampaignService::is_level_completed(const String &level_id) const { return save_data_.completed_levels.contains(level_id); }

bool CampaignService::is_level_unlocked(const String &level_id) const {
    const LevelUnlock *unlock = find_level_unlock(level_id);
    if (unlock == nullptr) {
        return false;
    }

    if (!unlock->requires_completed.is_empty() && !is_level_completed(unlock->requires_completed)) {
        return false;
    }
    return true;
}

bool CampaignService::can_claim_level_upgrade(const String &level_id) const { return get_claimed_upgrade_for_level(level_id).is_empty(); }

bool CampaignService::can_claim_rescue_draft(const String &level_id) const {
    if (level_id.is_empty() || level_id != get_frontier_level_id() || is_level_completed(level_id)) {
        return false;
    }

    const int next_threshold = get_next_rescue_draft_threshold(level_id);
    return next_threshold > 0 && save_data_.total_score >= next_threshold;
}

String CampaignService::get_claimed_upgrade_for_level(const String &level_id) const {
    if (const auto found = save_data_.claimed_level_upgrades.find(level_id); found != save_data_.claimed_level_upgrades.end()) {
        return found->second;
    }

    return {};
}

String CampaignService::get_frontier_level_id() const {
    for (const auto &unlock : catalog_.get_level_unlocks()) {
        if (is_level_unlocked(unlock.level_id) && !is_level_completed(unlock.level_id)) {
            return unlock.level_id;
        }
    }
    return {};
}

int CampaignService::get_highest_level_score(const String &level_id) const {
    if (const auto found = save_data_.best_level_scores.find(level_id); found != save_data_.best_level_scores.end()) {
        return found->second;
    }

    return 0;
}

int CampaignService::get_rescue_drafts_claimed(const String &level_id) const {
    if (const auto found = save_data_.claimed_rescue_drafts.find(level_id); found != save_data_.claimed_rescue_drafts.end()) {
        return std::max(0, found->second);
    }

    return 0;
}

int CampaignService::get_next_rescue_draft_threshold(const String &level_id) const {
    const LevelUnlock *unlock = find_level_unlock(level_id);
    if (unlock == nullptr) {
        return 0;
    }

    const int claimed_count = get_rescue_drafts_claimed(level_id);
    if (!std::cmp_less(claimed_count, unlock->rescue_thresholds.size())) {
        return 0;
    }

    return unlock->rescue_thresholds[static_cast<size_t>(claimed_count)];
}

UnitConfig CampaignService::get_effective_friendly_unit_config(const UnitConfig &base_config) const {
    UnitConfig effective_config = base_config;
    if (effective_config.side != UnitSide::FRIENDLY) {
        return effective_config;
    }

    for_each_owned_upgrade_effect(save_data_, upgrade_catalog_, [&effective_config](const UpgradeCardDefinition &, const UpgradeCardEffect &effect) {
        if (effect.unit_id != effective_config.name) {
            return;
        }

        if (effect.type == UpgradeEffectType::UNIT_HP_DELTA) {
            effective_config.hp += round_effect_delta(effect.value);
        } else if (effect.type == UpgradeEffectType::UNIT_RANGED_DAMAGE_DELTA) {
            effective_config.ranged_damage += round_effect_delta(effect.value);
        } else if (effect.type == UpgradeEffectType::UNIT_MOVE_SPEED_DELTA) {
            effective_config.move_speed_pixels_per_second += effect.value;
        }
    });

    effective_config.hp = std::max(effective_config.hp, 1);
    effective_config.ranged_damage = std::max(effective_config.ranged_damage, 1);
    effective_config.move_speed_pixels_per_second = std::max(effective_config.move_speed_pixels_per_second, static_cast<real_t>(1.0F));
    return effective_config;
}

int CampaignService::get_effective_starting_energy(int base) const {
    int bonus = 0;

    for_each_owned_upgrade_effect(save_data_, upgrade_catalog_, [&bonus](const UpgradeCardDefinition &, const UpgradeCardEffect &effect) {
        if (effect.type == UpgradeEffectType::STARTING_ENERGY_DELTA) {
            bonus += static_cast<int>(effect.value);
        }
    });

    return base + bonus;
}

int CampaignService::get_effective_energy_regen() const {
    int regen = 1;

    for_each_owned_upgrade_effect(save_data_, upgrade_catalog_, [&regen](const UpgradeCardDefinition &, const UpgradeCardEffect &effect) {
        if (effect.type == UpgradeEffectType::ENERGY_REGEN_DELTA) {
            regen += static_cast<int>(effect.value);
        }
    });

    return regen;
}

real_t CampaignService::get_effective_bounty_multiplier() const {
    real_t mult = 1.0F;

    for_each_owned_upgrade_effect(save_data_, upgrade_catalog_, [&mult](const UpgradeCardDefinition &, const UpgradeCardEffect &effect) {
        if (effect.type == UpgradeEffectType::BOUNTY_MULTIPLIER_DELTA) {
            mult += effect.value;
        }
    });

    return mult;
}

int CampaignService::get_effective_base_integrity(int base) const {
    int integrity_bonus = 0;

    for_each_owned_upgrade_effect(save_data_, upgrade_catalog_, [&integrity_bonus](const UpgradeCardDefinition &, const UpgradeCardEffect &effect) {
        if (effect.type == UpgradeEffectType::BASE_INTEGRITY_DELTA) {
            integrity_bonus += static_cast<int>(effect.value);
        }
    });

    return base + integrity_bonus;
}

int CampaignService::get_completed_level_count() const { return static_cast<int>(save_data_.completed_levels.size()); }

const LevelUnlock *CampaignService::find_level_unlock(const String &level_id) const {
    for (const auto &unlock : catalog_.get_level_unlocks()) {
        if (unlock.level_id == level_id) {
            return &unlock;
        }
    }
    return nullptr;
}

std::vector<UpgradeCardViewModel> CampaignService::build_upgrade_draft() const {
    std::vector<UpgradeCardViewModel> result;

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

    result.reserve(selected_cards.size());
    for (const UpgradeCardDefinition *card : selected_cards) {
        result.push_back(build_upgrade_card_view(*card));
    }

    return result;
}

std::vector<UpgradeCardViewModel> CampaignService::build_upgrade_draft_for_level(const String &level_id) const {
    if (!can_claim_level_upgrade(level_id)) {
        return {};
    }

    return build_upgrade_draft();
}

std::vector<UpgradeCardViewModel> CampaignService::build_rescue_draft_for_level(const String &level_id) const {
    if (!can_claim_rescue_draft(level_id)) {
        return {};
    }

    return build_upgrade_draft();
}

void CampaignService::add_score(int amount) { save_data_.total_score += amount; }

void CampaignService::mark_level_completed(const String &level_id, int level_score) {
    save_data_.completed_levels.insert(level_id);
    save_data_.best_level_scores[level_id] = std::max(level_score, get_highest_level_score(level_id));
}

bool CampaignService::claim_level_upgrade(const String &level_id, const String &upgrade_id) {
    if (level_id.is_empty() || upgrade_id.is_empty() || !can_claim_level_upgrade(level_id)) {
        return false;
    }

    const UpgradeCardDefinition *card = upgrade_catalog_.find_card(upgrade_id);
    if (card == nullptr || get_owned_upgrade_count(upgrade_id) >= card->max_picks) {
        return false;
    }

    save_data_.claimed_level_upgrades[level_id] = upgrade_id;
    grant_upgrade(upgrade_id);
    return true;
}

bool CampaignService::claim_rescue_draft(const String &level_id, const String &upgrade_id) {
    if (level_id.is_empty() || upgrade_id.is_empty() || !can_claim_rescue_draft(level_id)) {
        return false;
    }

    const UpgradeCardDefinition *card = upgrade_catalog_.find_card(upgrade_id);
    if (card == nullptr || get_owned_upgrade_count(upgrade_id) >= card->max_picks) {
        return false;
    }

    const int next_threshold = get_next_rescue_draft_threshold(level_id);
    if (next_threshold <= 0 || save_data_.total_score < next_threshold) {
        return false;
    }

    set_rescue_drafts_claimed(level_id, get_rescue_drafts_claimed(level_id) + 1);
    grant_upgrade(upgrade_id);
    return true;
}

UpgradeCardViewModel CampaignService::build_upgrade_card_view(const UpgradeCardDefinition &card) {
    return {
        .id = card.id,
        .name = card.name,
        .description = card.description,
        .emoji = card.emoji,
        .category = card.category,
    };
}

void CampaignService::grant_upgrade(const String &upgrade_id) {
    const UpgradeCardDefinition *card = upgrade_catalog_.find_card(upgrade_id);
    if (card == nullptr || get_owned_upgrade_count(upgrade_id) >= card->max_picks) {
        return;
    }

    ++save_data_.owned_upgrade_counts[upgrade_id];
}

int CampaignService::get_owned_upgrade_count(const String &upgrade_id) const {
    if (const auto found = save_data_.owned_upgrade_counts.find(upgrade_id); found != save_data_.owned_upgrade_counts.end()) {
        return found->second;
    }

    return 0;
}

void CampaignService::set_rescue_drafts_claimed(const String &level_id, int claimed_count) {
    const int clamped_count = std::max(0, claimed_count);
    save_data_.claimed_rescue_drafts[level_id] = clamped_count;
}

} // namespace defn
