#include "progression_catalog.h"

#include "variant_tools.h"
#include <algorithm>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
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

    Array level_arr = data.get("level_unlocks", Array());
    level_unlocks_.clear();
    for (const Variant &entry_var : level_arr) {
        const Dictionary entry = entry_var;
        LevelUnlock unlock;
        unlock.level_id = entry.get("level_id", String());

        const Variant required_level = entry.get("requires_completed", Variant());
        if (required_level.get_type() == Variant::STRING) {
            unlock.requires_completed = String(required_level);
        }

        const Array rescue_thresholds = entry.get("rescue_thresholds", Array());
        for (const Variant &threshold_variant : rescue_thresholds) {
            const int threshold = std::max(0, VariantTools::as_int(threshold_variant));
            if (threshold > 0) {
                unlock.rescue_thresholds.push_back(threshold);
            }
        }

        level_unlocks_.push_back(unlock);
    }

    UtilityFunctions::print("ProgressionCatalog: Loaded ", level_unlocks_.size(), " level unlocks");
    return true;
}

} // namespace defn