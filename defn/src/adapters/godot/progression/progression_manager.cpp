#include "progression_manager.h"

#include "content_validator.h"
#include "data_paths.h"
#include "progression_presentation.h"
#include "unit_data.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

std::string to_std_string(const String &value) { return value.utf8().get_data(); }

String to_godot_string(const std::string &value) { return {value.c_str()}; }

PackedStringArray to_packed_string_array(const std::vector<std::string> &values) {
    PackedStringArray result;
    for (const auto &value : values) {
        result.push_back(to_godot_string(value));
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

void apply_progression_unit_stats(UnitConfig &config, const ProgressionUnitStats &stats) {
    config.hp = stats.hp;
    config.ranged_damage = stats.ranged_damage;
    config.move_speed_pixels_per_second = stats.move_speed;
}

UpgradeCardViewModel to_godot_upgrade_card(const ProgressionUpgradeCardViewModel &card) {
    return {
        .id = to_godot_string(card.id),
        .name = to_godot_string(card.name),
        .description = to_godot_string(card.description),
        .emoji = to_godot_string(card.emoji),
        .category = to_godot_string(card.category),
        .owned_count = card.owned_count,
    };
}

std::vector<UpgradeCardViewModel> to_godot_upgrade_cards(const std::vector<ProgressionUpgradeCardViewModel> &cards) {
    std::vector<UpgradeCardViewModel> result;
    result.reserve(cards.size());
    for (const auto &card : cards) {
        result.push_back(to_godot_upgrade_card(card));
    }
    return result;
}

} // namespace

CampaignService *CampaignService::singleton_ = nullptr;

CampaignService::CampaignService() : save_repository_(DataPaths::SAVE_DATA), use_cases_(save_repository_, catalog_, upgrade_catalog_, random_) {}

void CampaignService::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_total_score"), &CampaignService::get_total_score);
    ClassDB::bind_method(D_METHOD("get_unlocked_units"), &CampaignService::get_unlocked_units_godot);
    ClassDB::bind_method(D_METHOD("get_unlocked_levels"), &CampaignService::get_unlocked_levels_godot);
    ClassDB::bind_method(D_METHOD("can_claim_level_upgrade", "level_id"), &CampaignService::can_claim_level_upgrade_godot);
    ClassDB::bind_method(D_METHOD("can_claim_rescue_draft", "level_id"), &CampaignService::can_claim_rescue_draft_godot);
    ClassDB::bind_method(D_METHOD("is_level_completed", "level_id"), &CampaignService::is_level_completed_godot);
    ClassDB::bind_method(D_METHOD("is_level_unlocked", "level_id"), &CampaignService::is_level_unlocked_godot);
    ClassDB::bind_method(D_METHOD("get_frontier_level_id"), &CampaignService::get_frontier_level_id_godot);
    ClassDB::bind_method(D_METHOD("get_current_level_id"), &CampaignService::get_current_level_id_godot);
    ClassDB::bind_method(D_METHOD("set_current_level_id", "level_id"), &CampaignService::set_current_level_id_godot);
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
    const auto loaded_save = save_repository_.load_profile();
    if (!loaded_save) {
        UtilityFunctions::print("CampaignService: No save file found, creating default");
        create_default_save();
        return;
    }

    save_data_ = *loaded_save;
    UtilityFunctions::print("CampaignService: Loaded save - total_score=", save_data_.total_score, ", completed_levels=", save_data_.completed_levels.size(),
                            ", owned_upgrades=", save_data_.owned_upgrade_counts.size());
}

void CampaignService::create_default_save() { save_data_ = use_cases_.load_campaign(); }

void CampaignService::save() {
    if (!use_cases_.save_campaign(save_data_)) {
        return;
    }

    UtilityFunctions::print("CampaignService: Save written - total_score=", save_data_.total_score);
}

std::vector<std::string> CampaignService::get_unlocked_units() const { return use_cases_.build_available_roster(save_data_); }

PackedStringArray CampaignService::get_unlocked_units_godot() const { return to_packed_string_array(get_unlocked_units()); }

PackedStringArray CampaignService::get_unlocked_levels_godot() const {
    return to_packed_string_array(defn::get_unlocked_levels(save_data_, catalog_.get_progression_level_unlocks()));
}

PackedStringArray CampaignService::get_owned_upgrades_godot() const { return to_packed_string_array(defn::get_owned_upgrades(save_data_)); }

bool CampaignService::has_upgrade_godot(const String &upgrade_id) const { return defn::has_upgrade(save_data_, to_std_string(upgrade_id)); }

bool CampaignService::is_level_completed(const std::string &level_id) const { return defn::is_level_completed(save_data_, level_id); }

bool CampaignService::is_level_completed_godot(const String &level_id) const { return is_level_completed(to_std_string(level_id)); }

bool CampaignService::is_level_unlocked(const std::string &level_id) const {
    return defn::is_level_unlocked(save_data_, catalog_.get_progression_level_unlocks(), level_id);
}

bool CampaignService::is_level_unlocked_godot(const String &level_id) const { return is_level_unlocked(to_std_string(level_id)); }

bool CampaignService::can_claim_level_upgrade(const std::string &level_id) const { return defn::can_claim_level_upgrade(save_data_, level_id); }

bool CampaignService::can_claim_level_upgrade_godot(const String &level_id) const { return can_claim_level_upgrade(to_std_string(level_id)); }

bool CampaignService::can_claim_rescue_draft(const std::string &level_id) const {
    return defn::can_claim_rescue_draft(save_data_, catalog_.get_progression_level_unlocks(), level_id);
}

bool CampaignService::can_claim_rescue_draft_godot(const String &level_id) const { return can_claim_rescue_draft(to_std_string(level_id)); }

String CampaignService::get_claimed_upgrade_for_level_godot(const String &level_id) const {
    return to_godot_string(defn::get_claimed_upgrade_for_level(save_data_, to_std_string(level_id)));
}

std::string CampaignService::get_frontier_level_id() const { return defn::get_frontier_level_id(save_data_, catalog_.get_progression_level_unlocks()); }

String CampaignService::get_frontier_level_id_godot() const { return to_godot_string(get_frontier_level_id()); }

int CampaignService::get_highest_level_score(const std::string &level_id) const { return defn::get_highest_level_score(save_data_, level_id); }

int CampaignService::get_highest_level_score_godot(const String &level_id) const { return get_highest_level_score(to_std_string(level_id)); }

int CampaignService::get_rescue_drafts_claimed_godot(const String &level_id) const {
    return defn::get_rescue_drafts_claimed(save_data_, to_std_string(level_id));
}

int CampaignService::get_next_rescue_draft_threshold_godot(const String &level_id) const {
    return defn::get_next_rescue_draft_threshold(save_data_, catalog_.get_progression_level_unlocks(), to_std_string(level_id));
}

String CampaignService::get_current_level_id_godot() const { return to_godot_string(current_level_id_); }

void CampaignService::set_current_level_id_godot(const String &level_id) {
    if (!select_level(to_std_string(level_id))) {
        UtilityFunctions::printerr("CampaignService: Refusing to select locked or unknown level: ", level_id);
    }
}

ProgressionUnitStats CampaignService::get_effective_friendly_unit_stats(const ProgressionUnitStats &base_stats) const {
    return apply_owned_upgrade_effects(save_data_, upgrade_catalog_.get_progression_upgrade_cards(), base_stats);
}

UnitConfig CampaignService::get_effective_friendly_unit_config(const UnitConfig &base_config) const {
    UnitConfig effective_config = base_config;
    apply_progression_unit_stats(effective_config, get_effective_friendly_unit_stats(to_progression_unit_stats(base_config)));
    return effective_config;
}

int CampaignService::get_effective_starting_energy(int base) const {
    return base + calculate_campaign_modifiers(save_data_, upgrade_catalog_.get_progression_upgrade_cards()).starting_energy_delta;
}

int CampaignService::get_effective_energy_regen() const {
    return calculate_campaign_modifiers(save_data_, upgrade_catalog_.get_progression_upgrade_cards()).energy_regen;
}

float CampaignService::get_effective_bounty_multiplier() const {
    return calculate_campaign_modifiers(save_data_, upgrade_catalog_.get_progression_upgrade_cards()).bounty_multiplier;
}

int CampaignService::get_effective_base_integrity(int base) const {
    return base + calculate_campaign_modifiers(save_data_, upgrade_catalog_.get_progression_upgrade_cards()).base_integrity_delta;
}

int CampaignService::get_completed_level_count() const { return defn::get_completed_level_count(save_data_); }

bool CampaignService::select_level(const std::string &level_id) {
    if (!use_cases_.can_select_level(save_data_, level_id)) {
        return false;
    }
    current_level_id_ = level_id;
    return true;
}

ProgressionMatchResult CampaignService::complete_level(const std::string &level_id, int level_score, bool victory) {
    return use_cases_.complete_level(save_data_, level_id, level_score, victory);
}

bool CampaignService::claim_upgrade(const ProgressionRewardClaim &claim) { return use_cases_.claim_upgrade(save_data_, claim); }

std::vector<std::string> CampaignService::build_new_unlock_descriptions(const std::vector<std::string> &level_ids) const {
    return ProgressionPresentation::describe_new_unlocks(level_ids);
}

ProgressionRewardViewModel CampaignService::build_reward_view_model(const ProgressionRewardDraft &draft) const {
    return ProgressionPresentation::build_reward_view_model(draft, upgrade_catalog_.get_upgrade_presentations());
}

std::vector<UpgradeCardViewModel> CampaignService::build_upgrade_draft_for_level_godot(const String &level_id) {
    return to_godot_upgrade_cards(build_reward_view_model(use_cases_.build_first_clear_reward_draft(save_data_, to_std_string(level_id))).available_upgrades);
}

std::vector<UpgradeCardViewModel> CampaignService::build_rescue_draft_for_level_godot(const String &level_id) {
    return to_godot_upgrade_cards(build_reward_view_model(use_cases_.build_rescue_reward_draft(save_data_, to_std_string(level_id))).available_upgrades);
}

std::vector<ProgressionUpgradeCardViewModel> CampaignService::build_owned_upgrade_cards() const {
    std::vector<ProgressionUpgradeCardViewModel> result;
    for (const auto &card : upgrade_catalog_.get_upgrade_presentations()) {
        const int owned = get_owned_upgrade_count(card.id);
        if (owned <= 0) {
            continue;
        }
        result.push_back(ProgressionPresentation::build_upgrade_card_view_model(card, owned));
    }
    return result;
}

std::vector<UpgradeCardViewModel> CampaignService::build_owned_upgrade_cards_godot() const { return to_godot_upgrade_cards(build_owned_upgrade_cards()); }

void CampaignService::add_score(int amount) { ProgressionUseCases::add_score(save_data_, amount); }

void CampaignService::mark_level_completed(const String &level_id, int level_score) {
    ProgressionUseCases::mark_level_completed(save_data_, to_std_string(level_id), level_score);
}

bool CampaignService::claim_level_upgrade(const String &level_id, const String &upgrade_id) {
    return claim_upgrade({.source = ProgressionRewardSource::FIRST_CLEAR, .level_id = to_std_string(level_id), .upgrade_id = to_std_string(upgrade_id)});
}

bool CampaignService::claim_rescue_draft(const String &level_id, const String &upgrade_id) {
    return claim_upgrade({.source = ProgressionRewardSource::RESCUE, .level_id = to_std_string(level_id), .upgrade_id = to_std_string(upgrade_id)});
}

int CampaignService::get_owned_upgrade_count(const std::string &upgrade_id) const { return defn::get_owned_upgrade_count(save_data_, upgrade_id); }

} // namespace defn