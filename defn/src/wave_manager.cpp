#include "wave_manager.h"
#include "grid_manager.h"
#include "level_loader.h"
#include "unit.h"
#include "unit_data.h"
#include "unit_factory.h"
#include <algorithm>
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
        const real_t spawn_y_pos = GridManager::random_belt_y();
        const real_t spawn_x_pos = grid->spawn_x();

        Unit *enemy = nullptr;
        if (unit_data_) {
            if (auto cfg = unit_data_->get_unit(spawn.type)) {
                enemy = UnitFactory::create(*cfg, Vector2(spawn_x_pos, spawn_y_pos));
            }
        }
        if (enemy == nullptr) {
            UtilityFunctions::printerr("WaveManager: Missing unit config for enemy type: ", spawn.type);
            ++next_spawn_idx;
            continue;
        }

        emit_signal("enemy_spawned", enemy);

        ++next_spawn_idx;
    }

    if (!completion_emitted_ && all_waves_spawned()) {
        completion_emitted_ = true;
        emit_signal("all_spawns_complete");
    }
}

void WaveManager::load_level(const String &path) {
    const auto loaded_level = LevelLoader::load(path);
    if (!loaded_level) {
        return;
    }

    level_definition_ = *loaded_level;
    all_spawns.clear();
    level_timer = 0.0;
    current_wave = 0;
    next_spawn_idx = 0;
    running = false;
    completion_emitted_ = false;

    for (const auto &wave_definition : level_definition_->waves) {
        for (const auto &spawn_definition : wave_definition.spawns) {
            FlatSpawn flat_spawn;
            flat_spawn.time = spawn_definition.time;
            flat_spawn.type = spawn_definition.type;
            flat_spawn.wave = wave_definition.wave_number;
            all_spawns.push_back(flat_spawn);
        }
    }

    // Sort all_spawns by time (should already be sorted but ensure)
    std::ranges::sort(all_spawns, [](const FlatSpawn &spawn_a, const FlatSpawn &spawn_b) { return spawn_a.time < spawn_b.time; });
}

void WaveManager::start() {
    level_timer = 0.0;
    next_spawn_idx = 0;
    current_wave = 0;
    completion_emitted_ = false;
    running = true;
}

void WaveManager::stop() { running = false; }

bool WaveManager::all_waves_spawned() const { return next_spawn_idx >= all_spawns.size(); }

} // namespace defn
