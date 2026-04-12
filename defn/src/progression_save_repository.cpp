#include "progression_save_repository.h"

#include "variant_tools.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace defn {

std::optional<PlayerProfile> ProgressionSaveRepository::load(const String &path) {
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
    PlayerProfile save_data;
    save_data.total_score = VariantTools::as_int(data.get("total_score", 0));

    Array completed = data.get("levels_completed", Array());
    for (const auto &level_var : completed) {
        save_data.completed_levels.insert(String(level_var));
    }

    Dictionary highest = data.get("best_level_scores", Dictionary());
    Array keys = highest.keys();
    for (const auto &key_var : keys) {
        const String key = key_var;
        const int score = VariantTools::as_int(highest[key]);
        save_data.best_level_scores[key] = score;
    }

    Dictionary owned_upgrades = data.get("owned_upgrade_counts", Dictionary());
    Array owned_upgrade_ids = owned_upgrades.keys();
    for (const Variant &upgrade_var : owned_upgrade_ids) {
        const String upgrade_id = upgrade_var;
        save_data.owned_upgrade_counts[upgrade_id] = std::max(0, VariantTools::as_int(owned_upgrades[upgrade_id]));
    }

    Dictionary claimed = data.get("claimed_level_upgrades", Dictionary());
    Array claimed_levels = claimed.keys();
    for (const Variant &level_var : claimed_levels) {
        const String level_id = level_var;
        const String upgrade_id = String(claimed[level_id]);
        save_data.claimed_level_upgrades[level_id] = upgrade_id;
    }

    Dictionary rescue_claimed = data.get("rescue_drafts_claimed", Dictionary());
    Array rescue_levels = rescue_claimed.keys();
    for (const Variant &level_var : rescue_levels) {
        const String level_id = level_var;
        const int claimed_count = VariantTools::as_int(rescue_claimed[level_id]);
        save_data.claimed_rescue_drafts[level_id] = std::max(0, claimed_count);
    }

    return save_data;
}

bool ProgressionSaveRepository::save(const String &path, const PlayerProfile &save_data) {
    Dictionary data;
    data["total_score"] = save_data.total_score;

    Array completed;
    for (const auto &level_id : save_data.completed_levels) {
        completed.push_back(level_id);
    }
    data["levels_completed"] = completed;

    Dictionary best_level_scores;
    for (const auto &[level_id, score] : save_data.best_level_scores) {
        best_level_scores[level_id] = score;
    }
    data["best_level_scores"] = best_level_scores;

    Dictionary owned_upgrade_counts;
    for (const auto &[upgrade_id, count] : save_data.owned_upgrade_counts) {
        owned_upgrade_counts[upgrade_id] = count;
    }
    data["owned_upgrade_counts"] = owned_upgrade_counts;

    Dictionary claimed_level_upgrades;
    for (const auto &[level_id, upgrade_id] : save_data.claimed_level_upgrades) {
        claimed_level_upgrades[level_id] = upgrade_id;
    }
    data["claimed_level_upgrades"] = claimed_level_upgrades;

    Dictionary rescue_drafts_claimed;
    for (const auto &[level_id, claimed_count] : save_data.claimed_rescue_drafts) {
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