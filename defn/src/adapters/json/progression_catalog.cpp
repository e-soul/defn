#include "progression_catalog.h"

#include "json_file_loader.h"
#include "variant_tools.h"
#include <algorithm>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

std::string to_std_string(const String &value) { return value.utf8().get_data(); }

} // namespace

bool ProgressionCatalog::load(const String &path) {
    const auto data = JsonFileLoader::load_dictionary(path, "ProgressionCatalog");
    if (!data) {
        return false;
    }

    const bool loaded = load_from_data(*data);
    if (loaded) {
        UtilityFunctions::print("ProgressionCatalog: Loaded ", level_unlocks_.size(), " level unlocks");
    }
    return loaded;
}

bool ProgressionCatalog::load_from_data(const Dictionary &data) {
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

    return true;
}

std::vector<ProgressionLevelUnlock> ProgressionCatalog::get_progression_level_unlocks() const {
    std::vector<ProgressionLevelUnlock> result;
    result.reserve(level_unlocks_.size());
    for (const auto &unlock : level_unlocks_) {
        result.push_back({
            .level_id = to_std_string(unlock.level_id),
            .requires_completed = to_std_string(unlock.requires_completed),
            .rescue_thresholds = unlock.rescue_thresholds,
        });
    }
    return result;
}

} // namespace defn