#include "spawn_scheduler.h"

#include "grid_manager.h"
#include "level_loader.h"
#include "unit.h"
#include "unit_data.h"
#include "unit_factory.h"

#include <algorithm>

#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

bool SpawnScheduler::load_level(const String &path) {
    const auto loaded_level = LevelLoader::load(path);
    if (!loaded_level) {
        return false;
    }

    level_definition_ = *loaded_level;
    all_spawns_.clear();
    level_timer_ = 0.0;
    current_wave_ = 0;
    next_spawn_idx_ = 0;
    running_ = false;
    completion_emitted_ = false;

    for (const auto &wave_definition : level_definition_->waves) {
        for (const auto &spawn_definition : wave_definition.spawns) {
            all_spawns_.push_back({
                .time = spawn_definition.time,
                .type = spawn_definition.type,
                .wave = wave_definition.wave_number,
            });
        }
    }

    std::ranges::sort(all_spawns_, [](const FlatSpawn &left, const FlatSpawn &right) { return left.time < right.time; });
    return true;
}

void SpawnScheduler::configure(const UnitDataLoader *unit_data, GridManager *grid) {
    unit_data_ = unit_data;
    grid_ = grid;
}

void SpawnScheduler::start() {
    level_timer_ = 0.0;
    next_spawn_idx_ = 0;
    current_wave_ = 0;
    completion_emitted_ = false;
    running_ = true;
}

void SpawnScheduler::stop() { running_ = false; }

SpawnSchedulerUpdate SpawnScheduler::update(double delta) {
    SpawnSchedulerUpdate update;
    if (!running_) {
        return update;
    }

    level_timer_ += delta;
    while (next_spawn_idx_ < all_spawns_.size() && level_timer_ >= all_spawns_[next_spawn_idx_].time) {
        const auto &spawn = all_spawns_[next_spawn_idx_];
        if (spawn.wave != current_wave_) {
            current_wave_ = spawn.wave;
            update.wave_changed = current_wave_;
        }

        if (unit_data_ == nullptr || grid_ == nullptr) {
            ++next_spawn_idx_;
            continue;
        }

        const auto config = unit_data_->get_unit(spawn.type);
        if (!config) {
            UtilityFunctions::printerr("SpawnScheduler: Missing unit config for enemy type: ", spawn.type);
            ++next_spawn_idx_;
            continue;
        }

        const real_t spawn_y_pos = GridManager::random_belt_y();
        const real_t spawn_x_pos = grid_->spawn_x();
        update.spawned_enemies.push_back(UnitFactory::create(*config, Vector2(spawn_x_pos, spawn_y_pos)));
        ++next_spawn_idx_;
    }

    if (!completion_emitted_ && all_waves_spawned()) {
        completion_emitted_ = true;
        update.all_spawns_completed = true;
    }

    return update;
}

} // namespace defn