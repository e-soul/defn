// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "level_loader.h"

#include "godot_string.h"
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
    level_definition.name = to_std_string(String(data.get("name", "")));
    level_definition.starting_core_resource = VariantTools::as_int(data.get("starting_core_resource", 100));
    level_definition.base_integrity = VariantTools::as_int(data.get("base_integrity", 3));
    const Array base_position = data.get("base_position", Array());
    if (base_position.size() >= 2) {
        level_definition.base_position_ratio = {
            .x = VariantTools::as_float(base_position[0]),
            .y = VariantTools::as_float(base_position[1]),
        };
    }
    const Array belt_width = data.get("belt_width", Array());
    if (belt_width.size() >= 2) {
        level_definition.belt_width_ratio = {
            .x = VariantTools::as_float(belt_width[0]),
            .y = VariantTools::as_float(belt_width[1]),
        };
    }
    level_definition.background_path = to_std_string(String(data.get("background", "")));

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
                .type = to_std_string(String(spawn_dict.get("type", "jackal"))),
            });
        }

        level_definition.waves.push_back(std::move(wave_definition));
    }

    return level_definition;
}

} // namespace defn
