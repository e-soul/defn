#ifndef LEVEL_DEFINITION_H
#define LEVEL_DEFINITION_H

#include <godot_cpp/variant/string.hpp>
#include <vector>

namespace defn {

using namespace godot;

struct SpawnDefinition {
    double time = 0.0;
    String type = "jackal";
};

struct WaveDefinition {
    int wave_number = 1;
    std::vector<SpawnDefinition> spawns;
};

struct LevelDefinition {
    int level_id = 0;
    String name;
    int starting_core_resource = 100;
    int base_integrity = 3;
    String background_path;
    std::vector<WaveDefinition> waves;
};

} // namespace defn

#endif