#include "progression_use_cases.h"

#include "unit_definition.h"

#include <algorithm>
#include <optional>
#include <ranges>

namespace defn {

namespace {

const ProgressionUpgradeCard *find_upgrade_card(const std::vector<ProgressionUpgradeCard> &cards, const std::string &upgrade_id) {
    for (const auto &card : cards) {
        if (card.id == upgrade_id) {
            return &card;
        }
    }
    return nullptr;
}

bool can_grant_upgrade(const ProgressionProfile &profile, const std::vector<ProgressionUpgradeCard> &cards, const std::string &upgrade_id) {
    const ProgressionUpgradeCard *card = find_upgrade_card(cards, upgrade_id);
    return card != nullptr && get_owned_upgrade_count(profile, upgrade_id) < card->max_picks;
}

void grant_upgrade(ProgressionProfile &profile, const std::string &upgrade_id) { ++profile.owned_upgrade_counts[upgrade_id]; }

bool contains_string(const std::vector<std::string> &values, const std::string &candidate) { return std::ranges::find(values, candidate) != values.end(); }

std::vector<std::string> find_new_unlocks(const PlayerProfile &before, const PlayerProfile &after, const std::vector<ProgressionLevelUnlock> &level_unlocks,
                                          const std::string &completed_level_id) {
    std::vector<std::string> result;
    if (completed_level_id.empty()) {
        return result;
    }

    const std::vector<std::string> before_unlocked = get_unlocked_levels(before, level_unlocks);
    for (const auto &unlock : level_unlocks) {
        if (unlock.requires_completed == completed_level_id && is_level_unlocked(after, level_unlocks, unlock.level_id) &&
            !contains_string(before_unlocked, unlock.level_id)) {
            result.push_back(unlock.level_id);
        }
    }
    return result;
}

std::string find_next_unlocked_level(const PlayerProfile &profile, const std::vector<ProgressionLevelUnlock> &level_unlocks,
                                     const std::string &current_level_id) {
    for (size_t index = 0; index < level_unlocks.size(); ++index) {
        if (level_unlocks[index].level_id == current_level_id && index + 1 < level_unlocks.size()) {
            const std::string &candidate = level_unlocks[index + 1].level_id;
            return is_level_unlocked(profile, level_unlocks, candidate) ? candidate : std::string();
        }
    }
    return {};
}

std::string find_idle_portrait(const UnitConfig &unit) {
    const auto idle = std::ranges::find_if(unit.animations, [](const auto &animation) { return animation.first == "idle"; });
    return idle == unit.animations.end() ? std::string() : idle->second.path_template;
}

bool effect_targets_entity(const ProgressionUpgradeEffect &effect, const ProgressionEntitySnapshot &entity) {
    if (entity.kind == ProgressionEntityKind::UNIT) {
        return effect.unit_id == entity.id &&
               (effect.type == ProgressionUpgradeEffectType::UNIT_HP_DELTA || effect.type == ProgressionUpgradeEffectType::UNIT_RANGED_DAMAGE_DELTA ||
                effect.type == ProgressionUpgradeEffectType::UNIT_MOVE_SPEED_DELTA || effect.type == ProgressionUpgradeEffectType::UNIT_UNLOCK);
    }
    if (entity.kind == ProgressionEntityKind::BASE) {
        return effect.type == ProgressionUpgradeEffectType::BASE_INTEGRITY_DELTA ||
               (effect.unit_id == entity.id &&
                (effect.type == ProgressionUpgradeEffectType::UNIT_HP_DELTA || effect.type == ProgressionUpgradeEffectType::UNIT_RANGED_DAMAGE_DELTA));
    }
    return effect.type == ProgressionUpgradeEffectType::STARTING_ENERGY_DELTA || effect.type == ProgressionUpgradeEffectType::ENERGY_REGEN_DELTA ||
           effect.type == ProgressionUpgradeEffectType::BOUNTY_MULTIPLIER_DELTA;
}

ProgressionUnitStats to_progression_stats(const UnitConfig &unit) {
    return {.unit_id = unit.name,
            .friendly = unit.side == UnitSide::FRIENDLY,
            .hp = unit.hp,
            .ranged_damage = unit.ranged_damage,
            .move_speed = unit.move_speed_pixels_per_second,
            .has_projectile_attack = unit.projectile_attack.has_value()};
}

ProgressionEntitySnapshot make_character_entity(const UnitConfig &unit, ProgressionEntityKind kind, bool unlocked, const PlayerProfile &profile,
                                                const std::vector<ProgressionUpgradeCard> &cards) {
    const ProgressionUnitStats effective_stats = apply_owned_upgrade_effects(profile, cards, to_progression_stats(unit));
    ProgressionEntitySnapshot entity{
        .id = unit.name, .kind = kind, .unlocked = unlocked, .description = unit.description, .portrait_path_template = find_idle_portrait(unit)};
    if (kind == ProgressionEntityKind::BASE) {
        const double fire_rate = unit.ranged_attack_period_seconds > 0.0 ? 1.0 / unit.ranged_attack_period_seconds : 0.0;
        const CampaignModifiers modifiers = calculate_campaign_modifiers(profile, cards);
        entity.stats = {
            {.id = "health", .base_value = static_cast<double>(unit.hp), .effective_value = static_cast<double>(effective_stats.hp)},
            {.id = "firepower", .base_value = static_cast<double>(unit.ranged_damage), .effective_value = static_cast<double>(effective_stats.ranged_damage)},
            {.id = "fire_rate", .base_value = fire_rate, .effective_value = fire_rate},
            {.id = "integrity_bonus", .contribution = static_cast<double>(modifiers.base_integrity_delta), .contribution_only = true}};
        return entity;
    }

    const bool uses_ranged_attack = unit.ranged_damage > 0;
    const int base_firepower = uses_ranged_attack ? unit.ranged_damage : unit.melee_damage;
    const int effective_firepower = uses_ranged_attack ? effective_stats.ranged_damage : unit.melee_damage;
    entity.stats = {{.id = "health", .base_value = static_cast<double>(unit.hp), .effective_value = static_cast<double>(effective_stats.hp)},
                    {.id = "firepower", .base_value = static_cast<double>(base_firepower), .effective_value = static_cast<double>(effective_firepower)},
                    {.id = "mobility", .base_value = unit.move_speed_pixels_per_second, .effective_value = effective_stats.move_speed},
                    {.id = "deploy_cost", .base_value = static_cast<double>(unit.cost), .effective_value = static_cast<double>(unit.cost)}};
    return entity;
}

ProgressionEntitySnapshot make_operations_entity(const CampaignModifiers &modifiers) {
    return {.id = "operations",
            .kind = ProgressionEntityKind::OPERATIONS,
            .unlocked = true,
            .description = "Campaign-wide command resources and mission support.",
            .stats = {{.id = "starting_energy_bonus", .contribution = static_cast<double>(modifiers.starting_energy_delta), .contribution_only = true},
                      {.id = "energy_generation", .base_value = 1.0, .effective_value = static_cast<double>(modifiers.energy_regen)},
                      {.id = "bounty_bonus", .contribution = static_cast<double>(modifiers.bounty_multiplier - 1.0F), .contribution_only = true}}};
}

bool card_unlocks_entity(const ProgressionUpgradeCard &card, const ProgressionEntitySnapshot &entity) {
    return std::ranges::any_of(
        card.effects, [&entity](const auto &effect) { return effect.type == ProgressionUpgradeEffectType::UNIT_UNLOCK && effect.unit_id == entity.id; });
}

void append_upgrade_relationships(ProgressionEntitySnapshot &entity, const PlayerProfile &profile, const std::vector<ProgressionUpgradeCard> &cards,
                                  const UpgradeCatalogPort &upgrade_catalog) {
    for (const auto &card : cards) {
        const int owned_count = get_owned_upgrade_count(profile, card.id);
        const bool targets = std::ranges::any_of(card.effects, [&entity](const auto &effect) { return effect_targets_entity(effect, entity); });
        if (targets && owned_count > 0) {
            const auto presentation = upgrade_catalog.find_upgrade_presentation(card.id);
            entity.contributing_upgrades.push_back(
                {.presentation = presentation.value_or(ProgressionUpgradePresentation{.id = card.id}), .owned_count = owned_count});
        }
        if (!entity.unlocked && entity.kind == ProgressionEntityKind::UNIT && card_unlocks_entity(card, entity)) {
            const auto presentation = upgrade_catalog.find_upgrade_presentation(card.id);
            entity.unlock_upgrade_name = presentation.has_value() && !presentation->name.empty() ? presentation->name : card.id;
        }
    }
}

} // namespace

void ProgressionUseCases::add_score(ProgressionProfile &profile, int amount) { profile.total_score += amount; }

void ProgressionUseCases::mark_level_completed(ProgressionProfile &profile, const std::string &level_id, int level_score) {
    profile.completed_levels.insert(level_id);
    profile.best_level_scores[level_id] = std::max(level_score, get_highest_level_score(profile, level_id));
}

bool ProgressionUseCases::claim_level_upgrade(ProgressionProfile &profile, const std::vector<ProgressionUpgradeCard> &cards, const std::string &level_id,
                                              const std::string &upgrade_id) {
    if (level_id.empty() || upgrade_id.empty() || !can_claim_level_upgrade(profile, level_id) || !can_grant_upgrade(profile, cards, upgrade_id)) {
        return false;
    }

    profile.claimed_level_upgrades[level_id] = upgrade_id;
    grant_upgrade(profile, upgrade_id);
    return true;
}

bool ProgressionUseCases::claim_rescue_draft(ProgressionProfile &profile, const std::vector<ProgressionLevelUnlock> &level_unlocks,
                                             const std::vector<ProgressionUpgradeCard> &cards, const std::string &level_id, const std::string &upgrade_id) {
    if (level_id.empty() || upgrade_id.empty() || !can_claim_rescue_draft(profile, level_unlocks, level_id) || !can_grant_upgrade(profile, cards, upgrade_id)) {
        return false;
    }

    const int next_threshold = get_next_rescue_draft_threshold(profile, level_unlocks, level_id);
    if (next_threshold <= 0 || profile.total_score < next_threshold) {
        return false;
    }

    profile.claimed_rescue_drafts[level_id] = get_rescue_drafts_claimed(profile, level_id) + 1;
    grant_upgrade(profile, upgrade_id);
    return true;
}

std::vector<std::string> ProgressionUseCases::build_upgrade_draft(const ProgressionProfile &profile, const std::vector<UpgradeDraftCard> &cards,
                                                                  const UpgradeDraftConfig &config, RandomSource &random) {
    return defn::build_upgrade_draft(profile, cards, config, random);
}

ProgressionCampaignUseCases::ProgressionCampaignUseCases(ProfileRepository &profile_repository, const ProgressionCatalogPort &progression_catalog,
                                                         const UpgradeCatalogPort &upgrade_catalog, RandomSource &random)
    : profile_repository_(profile_repository), progression_catalog_(progression_catalog), upgrade_catalog_(upgrade_catalog), random_(random) {}

PlayerProfile ProgressionCampaignUseCases::load_campaign() const {
    if (const std::optional<PlayerProfile> loaded_profile = profile_repository_.load_profile(); loaded_profile.has_value()) {
        return *loaded_profile;
    }

    PlayerProfile default_profile;
    profile_repository_.save_profile(default_profile);
    return default_profile;
}

bool ProgressionCampaignUseCases::save_campaign(const PlayerProfile &profile) const { return profile_repository_.save_profile(profile); }

bool ProgressionCampaignUseCases::can_select_level(const PlayerProfile &profile, const std::string &level_id) const {
    return is_level_unlocked(profile, progression_catalog_.get_progression_level_unlocks(), level_id);
}

ProgressionMatchResult ProgressionCampaignUseCases::complete_level(PlayerProfile &profile, const std::string &level_id, int level_score, bool victory) {
    const PlayerProfile before = profile;
    const std::vector<ProgressionLevelUnlock> level_unlocks = progression_catalog_.get_progression_level_unlocks();

    ProgressionUseCases::add_score(profile, level_score);
    if (victory) {
        ProgressionUseCases::mark_level_completed(profile, level_id, level_score);
    }

    ProgressionMatchResult result;
    result.new_total_score = profile.total_score;
    if (victory) {
        result.new_unlock_level_ids = find_new_unlocks(before, profile, level_unlocks, level_id);
        result.next_level_id = find_next_unlocked_level(profile, level_unlocks, level_id);
    }
    result.reward_draft = build_reward_draft(profile, level_id, victory);
    profile_repository_.save_profile(profile);
    return result;
}

ProgressionRewardDraft ProgressionCampaignUseCases::build_first_clear_reward_draft(const PlayerProfile &profile, const std::string &level_id) {
    if (!can_claim_level_upgrade(profile, level_id)) {
        return {};
    }

    return {
        .source = ProgressionRewardSource::FIRST_CLEAR,
        .level_id = level_id,
        .upgrade_ids = build_upgrade_ids(profile),
    };
}

ProgressionRewardDraft ProgressionCampaignUseCases::build_rescue_reward_draft(const PlayerProfile &profile, const std::string &level_id) {
    if (!can_claim_rescue_draft(profile, progression_catalog_.get_progression_level_unlocks(), level_id)) {
        return {};
    }

    return {
        .source = ProgressionRewardSource::RESCUE,
        .level_id = level_id,
        .upgrade_ids = build_upgrade_ids(profile),
    };
}

ProgressionRewardDraft ProgressionCampaignUseCases::build_reward_draft(const PlayerProfile &profile, const std::string &level_id, bool victory) {
    if (victory) {
        return build_first_clear_reward_draft(profile, level_id);
    }

    const std::string frontier_level_id = get_frontier_level_id(profile, progression_catalog_.get_progression_level_unlocks());
    if (frontier_level_id.empty()) {
        return {};
    }
    return build_rescue_reward_draft(profile, frontier_level_id);
}

bool ProgressionCampaignUseCases::claim_upgrade(PlayerProfile &profile, const ProgressionRewardClaim &claim) const {
    bool claimed = false;
    if (claim.source == ProgressionRewardSource::FIRST_CLEAR) {
        claimed = ProgressionUseCases::claim_level_upgrade(profile, upgrade_catalog_.get_progression_upgrade_cards(), claim.level_id, claim.upgrade_id);
    } else if (claim.source == ProgressionRewardSource::RESCUE) {
        claimed = ProgressionUseCases::claim_rescue_draft(profile, progression_catalog_.get_progression_level_unlocks(),
                                                          upgrade_catalog_.get_progression_upgrade_cards(), claim.level_id, claim.upgrade_id);
    }

    if (claimed) {
        profile_repository_.save_profile(profile);
    }
    return claimed;
}

std::vector<std::string> ProgressionCampaignUseCases::build_available_roster(const PlayerProfile &profile) const {
    return get_unlocked_units(profile, upgrade_catalog_.get_base_unit_ids(), upgrade_catalog_.get_progression_upgrade_cards());
}

ProgressionOverviewSnapshot ProgressionCampaignUseCases::build_progression_overview(const PlayerProfile &profile, const UnitCatalog &unit_catalog) const {
    const auto cards = upgrade_catalog_.get_progression_upgrade_cards();
    const auto unlocked_units = get_unlocked_units(profile, upgrade_catalog_.get_base_unit_ids(), cards);
    ProgressionOverviewSnapshot result;

    if (const auto base = unit_catalog.get_unit("base"); base.has_value()) {
        result.entities.push_back(make_character_entity(*base, ProgressionEntityKind::BASE, true, profile, cards));
    } else {
        result.entities.push_back({.id = "base", .kind = ProgressionEntityKind::BASE, .unlocked = true});
    }

    for (const auto &unit : unit_catalog.get_friendly_units()) {
        if (unit.name == "base") {
            continue;
        }
        const bool unlocked = std::ranges::find(unlocked_units, unit.name) != unlocked_units.end();
        result.entities.push_back(make_character_entity(unit, ProgressionEntityKind::UNIT, unlocked, profile, cards));
    }

    const CampaignModifiers modifiers = calculate_campaign_modifiers(profile, cards);
    result.entities.push_back(make_operations_entity(modifiers));

    for (auto &entity : result.entities) {
        append_upgrade_relationships(entity, profile, cards, upgrade_catalog_);
    }
    return result;
}

std::vector<std::string> ProgressionCampaignUseCases::build_upgrade_ids(const PlayerProfile &profile) {
    return ProgressionUseCases::build_upgrade_draft(profile, upgrade_catalog_.get_upgrade_draft_cards(), upgrade_catalog_.get_upgrade_draft_config(), random_);
}

} // namespace defn
