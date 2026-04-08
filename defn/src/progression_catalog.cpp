#include "progression_catalog.h"

#include "variant_tools.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

bool ProgressionCatalog::load(const String &path) {
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (!file.is_valid()) {
        UtilityFunctions::printerr("ProgressionCatalog: Failed to open: ", path);
        return false;
    }

    String json_text = file->get_as_text();
    file->close();

    Ref<JSON> json;
    json.instantiate();
    if (json->parse(json_text) != OK) {
        UtilityFunctions::printerr("ProgressionCatalog: JSON parse error: ", json->get_error_message());
        return false;
    }

    Dictionary data = json->get_data();

    Array unit_arr = data.get("unit_unlocks", Array());
    unit_unlocks_.clear();
    for (const auto &entry_var : unit_arr) {
        Dictionary entry = entry_var;
        unit_unlocks_.push_back({
            .unit_id = String(entry.get("unit_id", "")),
            .score_required = VariantTools::as_int(entry.get("score_required", 0)),
        });
    }

    Array level_arr = data.get("level_unlocks", Array());
    level_unlocks_.clear();
    for (const auto &entry_var : level_arr) {
        Dictionary entry = entry_var;
        LevelUnlock unlock{
            .level_id = String(entry.get("level_id", "")),
            .score_required = VariantTools::as_int(entry.get("score_required", 0)),
        };

        const Variant required_level = entry.get("requires_completed", Variant());
        if (required_level.get_type() == Variant::STRING) {
            unlock.requires_completed = String(required_level);
        }

        level_unlocks_.push_back(unlock);
    }

    Array upgrade_arr = data.get("upgrades", Array());
    upgrades_.clear();
    for (const auto &entry_var : upgrade_arr) {
        Dictionary entry = entry_var;
        upgrades_.push_back({
            .id = String(entry.get("id", "")),
            .score_required = VariantTools::as_int(entry.get("score_required", 0)),
            .type = String(entry.get("type", "")),
            .value = VariantTools::as_real(entry.get("value", 0.0)),
        });
    }

    UtilityFunctions::print("ProgressionCatalog: Loaded ", unit_unlocks_.size(), " unit unlocks, ", level_unlocks_.size(), " level unlocks, ", upgrades_.size(),
                            " upgrades");
    return true;
}

} // namespace defn