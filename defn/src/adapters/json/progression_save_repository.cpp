// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "progression_save_repository.h"

#include "godot_string.h"
#include "variant_tools.h"

#include <algorithm>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace defn {

namespace {

std::string read_string(const Variant &value) { return to_std_string(String(value)); }
int read_count(const Variant &value) { return std::max(0, VariantTools::as_int(value)); }

void read_map(const Dictionary &data, const char *name, auto &target, auto read_key, auto read_value) {
    const Dictionary source = data.get(name, Dictionary());
    for (const Variant &key : source.keys()) {
        target[read_key(key)] = read_value(source[key]);
    }
}

Variant save_value(const std::string &value) { return to_godot_string(value); }
Variant save_value(int value) { return value; }

Dictionary write_map(const auto &source) {
    Dictionary result;
    for (const auto &[key, value] : source) {
        result[save_value(key)] = save_value(value);
    }
    return result;
}

} // namespace

std::optional<PlayerProfile> ProgressionSaveRepository::load_profile() const { return load(path_); }

bool ProgressionSaveRepository::save_profile(const PlayerProfile &profile) const { return save(path_, profile); }

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
        save_data.completed_levels.insert(to_std_string(String(level_var)));
    }

    read_map(data, "best_level_scores", save_data.best_level_scores, read_string, VariantTools::as_int);
    read_map(data, "owned_upgrade_counts", save_data.owned_upgrade_counts, read_string, read_count);
    read_map(data, "claimed_level_upgrades", save_data.claimed_level_upgrades, read_string, read_string);
    read_map(data, "rescue_drafts_claimed", save_data.claimed_rescue_drafts, read_string, read_count);
    // Absent in a pre-endless save, which loads as an empty record rather than as a failure.
    read_map(data, "endless_best_wave", save_data.endless_best_wave, VariantTools::as_int, read_count);
    read_map(data, "endless_best_score", save_data.endless_best_score, VariantTools::as_int, read_count);

    return save_data;
}

bool ProgressionSaveRepository::save(const String &path, const PlayerProfile &save_data) {
    Dictionary data;
    data["total_score"] = save_data.total_score;

    Array completed;
    for (const auto &level_id : save_data.completed_levels) {
        completed.push_back(to_godot_string(level_id));
    }
    data["levels_completed"] = completed;

    data["best_level_scores"] = write_map(save_data.best_level_scores);
    data["owned_upgrade_counts"] = write_map(save_data.owned_upgrade_counts);
    data["claimed_level_upgrades"] = write_map(save_data.claimed_level_upgrades);
    data["rescue_drafts_claimed"] = write_map(save_data.claimed_rescue_drafts);
    data["endless_best_wave"] = write_map(save_data.endless_best_wave);
    data["endless_best_score"] = write_map(save_data.endless_best_score);

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