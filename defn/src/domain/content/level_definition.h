#ifndef LEVEL_DEFINITION_H
#define LEVEL_DEFINITION_H

#include <string>
#include <vector>

namespace defn {

struct SpawnDefinition {
    double time = 0.0;
    std::string type = "jackal";
};

struct WaveDefinition {
    int wave_number = 1;
    std::vector<SpawnDefinition> spawns;
};

struct LevelDefinition {
    int level_id = 0;
    std::string name;
    int starting_core_resource = 100;
    int base_integrity = 3;
    std::string background_path;
    std::vector<WaveDefinition> waves;
};

} // namespace defn

#endif