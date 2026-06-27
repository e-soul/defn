#include "progression_manager.h"

#include "content_validator.h"
#include "data_paths.h"
#include "progression_rules.h"
#include "upgrade_draft_builder.h"
#include "unit_data.h"

#include <algorithm>
#include <array>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

std::string to_std_string(const String &value) { return value.utf8().get_data(); }

String to_godot_string(const std::string &value) { return {value.c_str()}; }

ProgressionUpgradeEffectType to_progression_effect_type(UpgradeEffectType type) {
    switch (type) {
    case UpgradeEffectType::STARTING_ENERGY_DELTA:
        return ProgressionUpgradeEffectType::STARTING_ENERGY_DELTA;
    case UpgradeEffectType::ENERGY_REGEN_DELTA:
        return ProgressionUpgradeEffectType::ENERGY_REGEN_DELTA;
    case UpgradeEffectType::BOUNTY_MULTIPLIER_DELTA:
        return ProgressionUpgradeEffectType::BOUNTY_MULTIPLIER_DELTA;
    case UpgradeEffectType::BASE_INTEGRITY_DELTA:
        return ProgressionUpgradeEffectType::BASE_INTEGRITY_DELTA;
    case UpgradeEffectType::UNIT_HP_DELTA:
        return ProgressionUpgradeEffectType::UNIT_HP_DELTA;
    case UpgradeEffectType::UNIT_RANGED_DAMAGE_DELTA:
        return ProgressionUpgradeEffectType::UNIT_RANGED_DAMAGE_DELTA;
    case UpgradeEffectType::UNIT_MOVE_SPEED_DELTA:
        return ProgressionUpgradeEffectType::UNIT_MOVE_SPEED_DELTA;
    case UpgradeEffectType::UNIT_UNLOCK:
        return ProgressionUpgradeEffectType::UNIT_UNLOCK;
    }

    return ProgressionUpgradeEffectType::STARTING_ENERGY_DELTA;
}

ProgressionProfile to_progression_profile(const PlayerProfile &profile) {
    ProgressionProfile result;
    result.total_score = profile.total_score;
    for (const auto &level_id : profile.completed_levels) {
        result.completed_levels.insert(to_std_string(level_id));
    }
    for (const auto &[level_id, score] : profile.best_level_scores) {
        result.best_level_scores[to_std_string(level_id)] = score;
    }
    for (const auto &[upgrade_id, count] : profile.owned_upgrade_counts) {
        result.owned_upgrade_counts[to_std_string(upgrade_id)] = count;
    }
    for (const auto &[level_id, upgrade_id] : profile.claimed_level_upgrades) {
        result.claimed_level_upgrades[to_std_string(level_id)] = to_std_string(upgrade_id);
    }
    for (const auto &[level_id, count] : profile.claimed_rescue_drafts) {
        result.claimed_rescue_drafts[to_std_string(level_id)] = count;
    }
    return result;
}

std::vector<ProgressionLevelUnlock> to_progression_level_unlocks(const std::vector<LevelUnlock> &level_unlocks) {
    std::vector<ProgressionLevelUnlock> result;
    result.reserve(level_unlocks.size());
    for (const auto &unlock : level_unlocks) {
        result.push_back({
            .level_id = to_std_string(unlock.level_id),
            .requires_completed = to_std_string(unlock.requires_completed),
            .rescue_thresholds = unlock.rescue_thresholds,
        });
    }
    return result;
}

std::vector<std::string> to_progression_strings(const std::vector<String> &values) {
    std::vector<std::string> result;
    result.reserve(values.size());
    for (const auto &value : values) {
        result.push_back(to_std_string(value));
    }
    return result;
}

std::vector<ProgressionUpgradeCard> to_progression_upgrade_cards(const std::vector<UpgradeCardDefinition> &cards) {
    std::vector<ProgressionUpgradeCard> result;
    result.reserve(cards.size());
    for (const auto &card : cards) {
        ProgressionUpgradeCard progression_card;
        progression_card.id = to_std_string(card.id);
        progression_card.max_picks = card.max_picks;
        progression_card.effects.reserve(card.effects.size());
        for (const auto &effect : card.effects) {
            progression_card.effects.push_back({
                .type = to_progression_effect_type(effect.type),
                .value = static_cast<float>(effect.value),
                .unit_id = to_std_string(effect.unit_id),
            });
        }
        result.push_back(progression_card);
    }
    return result;
}

std::vector<UpgradeDraftCard> to_upgrade_draft_cards(const std::vector<UpgradeCardDefinition> &cards) {
    std::vector<UpgradeDraftCard> result;
    result.reserve(cards.size());
    for (const auto &card : cards) {
        UpgradeDraftCard draft_card;
        draft_card.id = to_std_string(card.id);
        draft_card.minimum_completed_levels = card.minimum_completed_levels;
        draft_card.weight = card.weight;
        draft_card.max_picks = card.max_picks;
        draft_card.grants_unit_unlock = card.grants_unit_unlock();
        draft_card.prerequisites.reserve(card.prerequisites.size());
        for (const auto &prerequisite : card.prerequisites) {
            draft_card.prerequisites.push_back(to_std_string(prerequisite));
        }
        result.push_back(draft_card);
    }
    return result;
}

ProgressionUnitStats to_progression_unit_stats(const UnitConfig &config) {
    return {
        .unit_id = to_std_string(config.name),
        .friendly = config.side == UnitSide::FRIENDLY,
        .hp = config.hp,
        .ranged_damage = config.ranged_damage,
        .move_speed = static_cast<float>(config.move_speed_pixels_per_second),
        .has_projectile_attack = config.projectile_attack.has_value(),
    };
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
    const ProgressionProfile profile = to_progression_profile(save_data_);
    for (const auto &unit_id : defn::get_unlocked_units(profile, to_progression_strings(upgrade_catalog_.get_base_units()),
                                                       to_progression_upgrade_cards(upgrade_catalog_.get_cards()))) {
        result.push_back(to_godot_string(unit_id));
    }
    return result;
}

PackedStringArray CampaignService::get_unlocked_levels() const {
    PackedStringArray result;
    const ProgressionProfile profile = to_progression_profile(save_data_);
    for (const auto &level_id : defn::get_unlocked_levels(profile, to_progression_level_unlocks(catalog_.get_level_unlocks()))) {
        result.push_back(to_godot_string(level_id));
    }
    return result;
}

PackedStringArray CampaignService::get_owned_upgrades() const {
    PackedStringArray result;
    const ProgressionProfile profile = to_progression_profile(save_data_);
    for (const auto &upgrade_id : defn::get_owned_upgrades(profile)) {
        result.push_back(to_godot_string(upgrade_id));
    }
    return result;
}

bool CampaignService::has_upgrade(const String &upgrade_id) const { return defn::has_upgrade(to_progression_profile(save_data_), to_std_string(upgrade_id)); }

bool CampaignService::is_level_completed(const String &level_id) const { return defn::is_level_completed(to_progression_profile(save_data_), to_std_string(level_id)); }

bool CampaignService::is_level_unlocked(const String &level_id) const {
    return defn::is_level_unlocked(to_progression_profile(save_data_), to_progression_level_unlocks(catalog_.get_level_unlocks()), to_std_string(level_id));
}

bool CampaignService::can_claim_level_upgrade(const String &level_id) const {
    return defn::can_claim_level_upgrade(to_progression_profile(save_data_), to_std_string(level_id));
}

bool CampaignService::can_claim_rescue_draft(const String &level_id) const {
    return defn::can_claim_rescue_draft(to_progression_profile(save_data_), to_progression_level_unlocks(catalog_.get_level_unlocks()), to_std_string(level_id));
}

String CampaignService::get_claimed_upgrade_for_level(const String &level_id) const {
    return to_godot_string(defn::get_claimed_upgrade_for_level(to_progression_profile(save_data_), to_std_string(level_id)));
}

String CampaignService::get_frontier_level_id() const {
    return to_godot_string(defn::get_frontier_level_id(to_progression_profile(save_data_), to_progression_level_unlocks(catalog_.get_level_unlocks())));
}

int CampaignService::get_highest_level_score(const String &level_id) const {
    return defn::get_highest_level_score(to_progression_profile(save_data_), to_std_string(level_id));
}

int CampaignService::get_rescue_drafts_claimed(const String &level_id) const {
    return defn::get_rescue_drafts_claimed(to_progression_profile(save_data_), to_std_string(level_id));
}

int CampaignService::get_next_rescue_draft_threshold(const String &level_id) const {
    return defn::get_next_rescue_draft_threshold(to_progression_profile(save_data_), to_progression_level_unlocks(catalog_.get_level_unlocks()),
                                                 to_std_string(level_id));
}

UnitConfig CampaignService::get_effective_friendly_unit_config(const UnitConfig &base_config) const {
    UnitConfig effective_config = base_config;
    const ProgressionUnitStats effective_stats = apply_owned_upgrade_effects(to_progression_profile(save_data_), to_progression_upgrade_cards(upgrade_catalog_.get_cards()),
                                                                             to_progression_unit_stats(base_config));
    effective_config.hp = effective_stats.hp;
    effective_config.ranged_damage = effective_stats.ranged_damage;
    effective_config.move_speed_pixels_per_second = effective_stats.move_speed;
    return effective_config;
}

int CampaignService::get_effective_starting_energy(int base) const {
    return base + calculate_campaign_modifiers(to_progression_profile(save_data_), to_progression_upgrade_cards(upgrade_catalog_.get_cards())).starting_energy_delta;
}

int CampaignService::get_effective_energy_regen() const {
    return calculate_campaign_modifiers(to_progression_profile(save_data_), to_progression_upgrade_cards(upgrade_catalog_.get_cards())).energy_regen;
}

real_t CampaignService::get_effective_bounty_multiplier() const {
    return calculate_campaign_modifiers(to_progression_profile(save_data_), to_progression_upgrade_cards(upgrade_catalog_.get_cards())).bounty_multiplier;
}

int CampaignService::get_effective_base_integrity(int base) const {
    return base + calculate_campaign_modifiers(to_progression_profile(save_data_), to_progression_upgrade_cards(upgrade_catalog_.get_cards())).base_integrity_delta;
}

int CampaignService::get_completed_level_count() const { return defn::get_completed_level_count(to_progression_profile(save_data_)); }

std::vector<UpgradeCardViewModel> CampaignService::build_upgrade_draft() const {
    std::vector<UpgradeCardViewModel> result;
    StdRandomSource random;
    const std::vector<std::string> selected_card_ids = defn::build_upgrade_draft(
        to_progression_profile(save_data_), to_upgrade_draft_cards(upgrade_catalog_.get_cards()),
        {.draft_size = upgrade_catalog_.get_draft_size(), .reserve_unit_slot = upgrade_catalog_.should_reserve_unit_slot()}, random);

    result.reserve(selected_card_ids.size());
    for (const auto &card_id : selected_card_ids) {
        const UpgradeCardDefinition *card = upgrade_catalog_.find_card(to_godot_string(card_id));
        if (card != nullptr) {
            result.push_back(build_upgrade_card_view(*card));
        }
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

std::vector<UpgradeCardViewModel> CampaignService::build_owned_upgrade_cards() const {
    std::vector<UpgradeCardViewModel> result;
    for (const auto &card : upgrade_catalog_.get_cards()) {
        const int owned = get_owned_upgrade_count(card.id);
        if (owned <= 0) {
            continue;
        }

        UpgradeCardViewModel view = build_upgrade_card_view(card);
        view.owned_count = owned;
        result.push_back(view);
    }
    return result;
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
    if (card == nullptr || defn::get_owned_upgrade_count(to_progression_profile(save_data_), to_std_string(upgrade_id)) >= card->max_picks) {
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
    if (card == nullptr || defn::get_owned_upgrade_count(to_progression_profile(save_data_), to_std_string(upgrade_id)) >= card->max_picks) {
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
    if (card == nullptr || defn::get_owned_upgrade_count(to_progression_profile(save_data_), to_std_string(upgrade_id)) >= card->max_picks) {
        return;
    }

    ++save_data_.owned_upgrade_counts[upgrade_id];
}

int CampaignService::get_owned_upgrade_count(const String &upgrade_id) const {
    return defn::get_owned_upgrade_count(to_progression_profile(save_data_), to_std_string(upgrade_id));
}

void CampaignService::set_rescue_drafts_claimed(const String &level_id, int claimed_count) {
    const int clamped_count = std::max(0, claimed_count);
    save_data_.claimed_rescue_drafts[level_id] = clamped_count;
}

} // namespace defn
