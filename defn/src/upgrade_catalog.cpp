#include "upgrade_catalog.h"

#include "variant_tools.h"

#include <algorithm>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

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
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (!file.is_valid()) {
        UtilityFunctions::printerr("UpgradeCatalog: Failed to open: ", path);
        return false;
    }

    const String json_text = file->get_as_text();
    file->close();

    Ref<JSON> json;
    json.instantiate();
    if (json->parse(json_text) != OK) {
        UtilityFunctions::printerr("UpgradeCatalog: JSON parse error: ", json->get_error_message());
        return false;
    }

    const Dictionary data = json->get_data();
    const bool loaded = load_from_data(data);
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

} // namespace defn