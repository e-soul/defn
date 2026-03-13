#include "wave_manager.h"
#include "hostile.h"
#include "grid_manager.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>

namespace defn {

WaveManager::WaveManager() = default;

void WaveManager::_bind_methods() {
    ADD_SIGNAL(MethodInfo("enemy_spawned", PropertyInfo(Variant::OBJECT, "enemy")));
    ADD_SIGNAL(MethodInfo("wave_changed", PropertyInfo(Variant::INT, "wave_number")));
    ADD_SIGNAL(MethodInfo("all_spawns_complete"));
}

void WaveManager::_ready() {}

void WaveManager::_process(double delta) {
    if (!running) return;

    level_timer += delta;

    while (next_spawn_idx < all_spawns.size() && level_timer >= all_spawns[next_spawn_idx].time) {
        auto &spawn = all_spawns[next_spawn_idx];

        // Update wave counter
        if (spawn.wave != current_wave) {
            current_wave = spawn.wave;
            emit_signal("wave_changed", current_wave);
        }

        // Create the hostile
        auto *enemy = memnew(Hostile);
        double y = GridManager::random_belt_y();
        double x = GridManager::spawn_x();
        enemy->set_position(Vector2(x, y));

        emit_signal("enemy_spawned", enemy);

        ++next_spawn_idx;
    }

    if (all_waves_spawned()) {
        emit_signal("all_spawns_complete");
    }
}

void WaveManager::load_level(const String &path) {
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (!file.is_valid()) {
        UtilityFunctions::printerr("WaveManager: Failed to open level file: ", path);
        return;
    }

    String json_text = file->get_as_text();
    file->close();

    Ref<JSON> json;
    json.instantiate();
    Error err = json->parse(json_text);
    if (err != OK) {
        UtilityFunctions::printerr("WaveManager: JSON parse error: ", json->get_error_message());
        return;
    }

    Dictionary data = json->get_data();
    starting_aether = static_cast<int>(data.get("starting_aether", 100));
    base_integrity_max = static_cast<int>(data.get("base_integrity", 3));

    Array wave_array = data.get("waves", Array());
    waves.clear();
    all_spawns.clear();

    for (int w = 0; w < wave_array.size(); ++w) {
        Dictionary wave_dict = wave_array[w];
        WaveData wd;
        wd.wave_number = static_cast<int>(wave_dict.get("wave_number", w + 1));

        Array spawns_array = wave_dict.get("spawns", Array());
        for (int s = 0; s < spawns_array.size(); ++s) {
            Dictionary spawn_dict = spawns_array[s];
            SpawnEvent se;
            se.time = static_cast<double>(spawn_dict.get("time", 0.0));
            se.type = String(spawn_dict.get("type", "grunt"));
            wd.spawns.push_back(se);

            FlatSpawn fs;
            fs.time = se.time;
            fs.type = se.type;
            fs.wave = wd.wave_number;
            all_spawns.push_back(fs);
        }

        waves.push_back(wd);
    }

    // Sort all_spawns by time (should already be sorted but ensure)
    std::sort(all_spawns.begin(), all_spawns.end(),
              [](const FlatSpawn &a, const FlatSpawn &b) { return a.time < b.time; });
}

void WaveManager::start() {
    level_timer = 0.0;
    next_spawn_idx = 0;
    current_wave = 0;
    running = true;
}

void WaveManager::stop() {
    running = false;
}

bool WaveManager::all_waves_spawned() const {
    return next_spawn_idx >= all_spawns.size();
}

} // namespace defn
