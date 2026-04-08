#include "progression_save_repository.h"

#include "variant_tools.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

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
    save_data.total_score = VariantTools::as_int(data.get("total_score", 0));

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

    return save_data;
}

bool ProgressionSaveRepository::save(const String &path, const ProgressionSaveData &save_data) {
    Dictionary data;
    data["version"] = 1;
    data["total_score"] = save_data.total_score;

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