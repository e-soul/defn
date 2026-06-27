#include "level_loader.h"

#include "json_file_loader.h"
#include "variant_tools.h"

namespace defn {

std::optional<LevelDefinition> LevelLoader::load(const String &path) {
    const auto data = JsonFileLoader::load_dictionary(path, "LevelLoader");
    return data ? load_from_data(*data) : std::nullopt;
}

std::optional<LevelDefinition> LevelLoader::load_from_data(const Dictionary &data) {
    LevelDefinition level_definition;
    level_definition.level_id = VariantTools::as_int(data.get("level_id", 0));
    level_definition.name = String(data.get("name", ""));
    level_definition.starting_core_resource = VariantTools::as_int(data.get("starting_core_resource", 100));
    level_definition.base_integrity = VariantTools::as_int(data.get("base_integrity", 3));
    level_definition.background_path = String(data.get("background", ""));

    Array wave_array = data.get("waves", Array());
    for (int wave_idx = 0; wave_idx < wave_array.size(); ++wave_idx) {
        Dictionary wave_dict = wave_array[wave_idx];
        WaveDefinition wave_definition;
        wave_definition.wave_number = VariantTools::as_int(wave_dict.get("wave_number", wave_idx + 1));

        Array spawn_array = wave_dict.get("spawns", Array());
        for (const auto &spawn_value : spawn_array) {
            Dictionary spawn_dict = spawn_value;
            wave_definition.spawns.push_back({
                .time = VariantTools::as_double(spawn_dict.get("time", 0.0)),
                .type = String(spawn_dict.get("type", "jackal")),
            });
        }

        level_definition.waves.push_back(std::move(wave_definition));
    }

    return level_definition;
}

} // namespace defn