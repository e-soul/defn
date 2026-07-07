#include "upgrade_catalog.h"

#include "json_file_loader.h"
#include "variant_tools.h"

#include <algorithm>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
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

ProgressionUpgradePresentation to_progression_upgrade_presentation(const UpgradeCardDefinition &card) {
    return {
        .id = to_std_string(card.id),
        .name = to_std_string(card.name),
        .description = to_std_string(card.description),
        .emoji = to_std_string(card.emoji),
        .category = to_std_string(card.category),
    };
}

} // namespace

bool try_parse_upgrade_effect_type(const String &value, UpgradeEffectType &out_type) {
    const String normalized = value.to_lower();
    if (normalized == "starting_energy_delta") {
        out_type = UpgradeEffectType::STARTING_ENERGY_DELTA;
        return true;
    }
    if (normalized == "energy_regen_delta") {
        out_type = UpgradeEffectType::ENERGY_REGEN_DELTA;
        return true;
    }
    if (normalized == "bounty_multiplier_delta") {
        out_type = UpgradeEffectType::BOUNTY_MULTIPLIER_DELTA;
        return true;
    }
    if (normalized == "base_integrity_delta") {
        out_type = UpgradeEffectType::BASE_INTEGRITY_DELTA;
        return true;
    }
    if (normalized == "unit_hp_delta") {
        out_type = UpgradeEffectType::UNIT_HP_DELTA;
        return true;
    }
    if (normalized == "unit_ranged_damage_delta") {
        out_type = UpgradeEffectType::UNIT_RANGED_DAMAGE_DELTA;
        return true;
    }
    if (normalized == "unit_move_speed_delta") {
        out_type = UpgradeEffectType::UNIT_MOVE_SPEED_DELTA;
        return true;
    }
    if (normalized == "unit_unlock") {
        out_type = UpgradeEffectType::UNIT_UNLOCK;
        return true;
    }
    return false;
}

bool UpgradeCardDefinition::grants_unit_unlock() const {
    return std::ranges::any_of(effects, [](const UpgradeCardEffect &effect) { return effect.type == UpgradeEffectType::UNIT_UNLOCK; });
}

bool UpgradeCatalog::load(const String &path) {
    const auto data = JsonFileLoader::load_dictionary(path, "UpgradeCatalog");
    if (!data) {
        return false;
    }

    const bool loaded = load_from_data(*data);
    if (loaded) {
        UtilityFunctions::print("UpgradeCatalog: Loaded ", cards_.size(), " upgrade cards");
    }
    return loaded;
}

bool UpgradeCatalog::load_from_data(const Dictionary &data) {
    draft_size_ = std::max(1, VariantTools::as_int(data.get("draft_size", 3)));
    reserve_unit_slot_ = VariantTools::as_bool(data.get("reserve_unit_slot", true));

    base_units_.clear();
    const Array base_units = data.get("base_units", Array());
    for (const Variant &unit_variant : base_units) {
        const String unit_id = String(unit_variant);
        if (!unit_id.is_empty()) {
            base_units_.push_back(unit_id);
        }
    }

    cards_.clear();
    const Array card_array = data.get("cards", Array());
    for (const Variant &card_variant : card_array) {
        const Dictionary card_dict = card_variant;

        UpgradeCardDefinition card;
        card.id = String(card_dict.get("id", ""));
        card.name = String(card_dict.get("name", ""));
        card.description = String(card_dict.get("description", ""));
        card.emoji = String(card_dict.get("emoji", ""));
        card.category = String(card_dict.get("category", ""));
        card.minimum_completed_levels = VariantTools::as_int(card_dict.get("minimum_completed_levels", 0));
        card.weight = std::max(1, VariantTools::as_int(card_dict.get("weight", 1)));
        card.max_picks = std::max(1, VariantTools::as_int(card_dict.get("max_picks", 1)));

        const Array prerequisite_array = card_dict.get("prerequisites", Array());
        for (const Variant &prerequisite_variant : prerequisite_array) {
            const String prerequisite = String(prerequisite_variant);
            if (!prerequisite.is_empty()) {
                card.prerequisites.push_back(prerequisite);
            }
        }

        const Array effect_array = card_dict.get("effects", Array());
        for (const Variant &effect_variant : effect_array) {
            const Dictionary effect_dict = effect_variant;
            UpgradeEffectType effect_type = UpgradeEffectType::STARTING_ENERGY_DELTA;
            if (!try_parse_upgrade_effect_type(String(effect_dict.get("type", "")), effect_type)) {
                continue;
            }

            card.effects.push_back({
                .type = effect_type,
                .value = VariantTools::as_real(effect_dict.get("value", 0.0)),
                .unit_id = String(effect_dict.get("unit_id", "")),
            });
        }

        if (!card.id.is_empty() && !card.name.is_empty() && !card.effects.empty()) {
            cards_.push_back(card);
        }
    }

    return true;
}

const UpgradeCardDefinition *UpgradeCatalog::find_card(const String &card_id) const {
    for (const auto &card : cards_) {
        if (card.id == card_id) {
            return &card;
        }
    }
    return nullptr;
}

std::vector<std::string> UpgradeCatalog::get_base_unit_ids() const {
    std::vector<std::string> result;
    result.reserve(base_units_.size());
    for (const auto &unit_id : base_units_) {
        result.push_back(to_std_string(unit_id));
    }
    return result;
}

std::vector<ProgressionUpgradeCard> UpgradeCatalog::get_progression_upgrade_cards() const {
    std::vector<ProgressionUpgradeCard> result;
    result.reserve(cards_.size());
    for (const auto &card : cards_) {
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

std::vector<UpgradeDraftCard> UpgradeCatalog::get_upgrade_draft_cards() const {
    std::vector<UpgradeDraftCard> result;
    result.reserve(cards_.size());
    for (const auto &card : cards_) {
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

UpgradeDraftConfig UpgradeCatalog::get_upgrade_draft_config() const { return {.draft_size = draft_size_, .reserve_unit_slot = reserve_unit_slot_}; }

std::optional<ProgressionUpgradePresentation> UpgradeCatalog::find_upgrade_presentation(const std::string &upgrade_id) const {
    if (const UpgradeCardDefinition *card = find_card(to_godot_string(upgrade_id)); card != nullptr) {
        return to_progression_upgrade_presentation(*card);
    }
    return std::nullopt;
}

std::vector<ProgressionUpgradePresentation> UpgradeCatalog::get_upgrade_presentations() const {
    std::vector<ProgressionUpgradePresentation> result;
    result.reserve(cards_.size());
    for (const auto &card : cards_) {
        result.push_back(to_progression_upgrade_presentation(card));
    }
    return result;
}

} // namespace defn