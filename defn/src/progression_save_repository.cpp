#include "progression_save_repository.h"

#include "variant_tools.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace defn {

namespace {

constexpr int CURRENT_SAVE_VERSION = 3;

} // namespace

std::optional<ProgressionSaveData> ProgressionSaveRepository::load(const String &path) {
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (!file.is_valid()) {
        return std::nullopt;
    }

    String json_text = file->get_as_text();
    file->close();

    Ref<JSON> json;
    json.instantiate();
    if (json->parse(json_text) != OK) {
        UtilityFunctions::printerr("ProgressionSaveRepository: Save file parse error: ", json->get_error_message());
        return std::nullopt;
    }

    Dictionary data = json->get_data();
    ProgressionSaveData save_data;
    save_data.schema_version = VariantTools::as_int(data.get("version", 1));
    save_data.total_score = VariantTools::as_int(data.get("total_score", 0));
    save_data.rescue_points_bank = VariantTools::as_int(data.get("rescue_points_bank", 0));

    Array completed = data.get("levels_completed", Array());
    for (const auto &level_var : completed) {
        save_data.levels_completed.push_back(String(level_var));
    }

    Dictionary highest = data.get("highest_level_score", Dictionary());
    Array keys = highest.keys();
    for (const auto &key_var : keys) {
        const String key = key_var;
        const int score = VariantTools::as_int(highest[key]);
        save_data.highest_level_scores.emplace_back(key, score);
    }

    Array owned_upgrades = data.get("owned_upgrades", Array());
    for (const Variant &upgrade_var : owned_upgrades) {
        save_data.owned_upgrades.push_back(String(upgrade_var));
    }

    Dictionary claimed = data.get("claimed_level_upgrades", Dictionary());
    Array claimed_levels = claimed.keys();
    for (const Variant &level_var : claimed_levels) {
        const String level_id = level_var;
        const String upgrade_id = String(claimed[level_id]);
        save_data.claimed_level_upgrades.emplace_back(level_id, upgrade_id);
    }

    Dictionary rescue_claimed = data.get("rescue_drafts_claimed", Dictionary());
    Array rescue_levels = rescue_claimed.keys();
    for (const Variant &level_var : rescue_levels) {
        const String level_id = level_var;
        const int claimed_count = VariantTools::as_int(rescue_claimed[level_id]);
        save_data.rescue_drafts_claimed.emplace_back(level_id, claimed_count);
    }

    return save_data;
}

bool ProgressionSaveRepository::save(const String &path, const ProgressionSaveData &save_data) {
    Dictionary data;
    data["version"] = CURRENT_SAVE_VERSION;
    data["total_score"] = save_data.total_score;
    data["rescue_points_bank"] = save_data.rescue_points_bank;

    Array completed;
    for (const auto &level_id : save_data.levels_completed) {
        completed.push_back(level_id);
    }
    data["levels_completed"] = completed;

    Dictionary highest;
    for (const auto &[level_id, score] : save_data.highest_level_scores) {
        highest[level_id] = score;
    }
    data["highest_level_score"] = highest;

    Array owned_upgrades;
    for (const auto &upgrade_id : save_data.owned_upgrades) {
        owned_upgrades.push_back(upgrade_id);
    }
    data["owned_upgrades"] = owned_upgrades;

    Dictionary claimed_level_upgrades;
    for (const auto &[level_id, upgrade_id] : save_data.claimed_level_upgrades) {
        claimed_level_upgrades[level_id] = upgrade_id;
    }
    data["claimed_level_upgrades"] = claimed_level_upgrades;

    Dictionary rescue_drafts_claimed;
    for (const auto &[level_id, claimed_count] : save_data.rescue_drafts_claimed) {
        rescue_drafts_claimed[level_id] = claimed_count;
    }
    data["rescue_drafts_claimed"] = rescue_drafts_claimed;

    const String json_text = JSON::stringify(data, "  ");
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
    if (!file.is_valid()) {
        UtilityFunctions::printerr("ProgressionSaveRepository: Failed to write save file");
        return false;
    }

    file->store_string(json_text);
    file->close();
    return true;
}

} // namespace defn