#include "wave_manager.h"
#include "grid_manager.h"
#include "unit.h"
#include "unit_data.h"
#include <algorithm>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

WaveManager::WaveManager() = default;

void WaveManager::_bind_methods() {
    ADD_SIGNAL(MethodInfo("enemy_spawned", PropertyInfo(Variant::OBJECT, "enemy")));
    ADD_SIGNAL(MethodInfo("wave_changed", PropertyInfo(Variant::INT, "wave_number")));
    ADD_SIGNAL(MethodInfo("all_spawns_complete"));
}

void WaveManager::_ready() {}

void WaveManager::_process(double delta) {
    if (!running) {
        return;
    }

    auto *grid = GridManager::get_singleton();

    level_timer += delta;

    while (next_spawn_idx < all_spawns.size() && level_timer >= all_spawns[next_spawn_idx].time) {
        auto &spawn = all_spawns[next_spawn_idx];

        // Update wave counter
        if (spawn.wave != current_wave) {
            current_wave = spawn.wave;
            emit_signal("wave_changed", current_wave);
        }

        // Create the hostile with data-driven config
        auto *enemy = memnew(Unit);
        if (unit_data_) {
            if (auto cfg = unit_data_->get_unit(spawn.type)) {
                enemy->set_unit_config(*cfg);
            }
        }
        double spawn_y_pos = grid->random_belt_y();
        double spawn_x_pos = grid->spawn_x();
        enemy->set_position(Vector2(static_cast<real_t>(spawn_x_pos), static_cast<real_t>(spawn_y_pos)));

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
    starting_core_resource = static_cast<int>(data.get("starting_core_resource", 100));
    base_integrity_max = static_cast<int>(data.get("base_integrity", 3));

    Array wave_array = data.get("waves", Array());
    waves.clear();
    all_spawns.clear();

    for (int wave_idx = 0; wave_idx < wave_array.size(); ++wave_idx) {
        Dictionary wave_dict = wave_array[wave_idx];
        WaveData wave_data;
        wave_data.wave_number = static_cast<int>(wave_dict.get("wave_number", wave_idx + 1));

        Array spawns_array = wave_dict.get("spawns", Array());
        for (const auto &spawn_val : spawns_array) {
            Dictionary spawn_dict = spawn_val;
            SpawnEvent spawn_event;
            spawn_event.time = static_cast<double>(spawn_dict.get("time", 0.0));
            spawn_event.type = String(spawn_dict.get("type", "grunt"));
            wave_data.spawns.push_back(spawn_event);

            FlatSpawn flat_spawn;
            flat_spawn.time = spawn_event.time;
            flat_spawn.type = spawn_event.type;
            flat_spawn.wave = wave_data.wave_number;
            all_spawns.push_back(flat_spawn);
        }

        waves.push_back(wave_data);
    }

    // Sort all_spawns by time (should already be sorted but ensure)
    std::ranges::sort(all_spawns, [](const FlatSpawn &spawn_a, const FlatSpawn &spawn_b) { return spawn_a.time < spawn_b.time; });
}

void WaveManager::start() {
    level_timer = 0.0;
    next_spawn_idx = 0;
    current_wave = 0;
    running = true;
}

void WaveManager::stop() { running = false; }

bool WaveManager::all_waves_spawned() const { return next_spawn_idx >= all_spawns.size(); }

} // namespace defn
