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

        const Variant rescue_variant = entry.get("rescue", Variant());
        if (rescue_variant.get_type() == Variant::DICTIONARY) {
            const Dictionary rescue_dict = rescue_variant;
            unlock.rescue.point_gain_multiplier = std::max(0.0F, VariantTools::as_real(rescue_dict.get("point_gain_multiplier", 0.0)));

            const Array draft_costs = rescue_dict.get("draft_costs", Array());
            for (const Variant &cost_variant : draft_costs) {
                const int cost = std::max(0, VariantTools::as_int(cost_variant));
                if (cost > 0) {
                    unlock.rescue.draft_costs.push_back(cost);
                }
            }
        }

        level_unlocks_.push_back(unlock);
    }

    UtilityFunctions::print("ProgressionCatalog: Loaded ", level_unlocks_.size(), " level unlocks");
    return true;
}

} // namespace defn