#include "spawn_scheduler.h"

#include "level_loader.h"
#include "unit_data.h"

#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

std::string to_std_string(const String &value) { return value.utf8().get_data(); }

String to_godot_string(const std::string &value) { return {value.c_str()}; }

SpawnTimelineDefinition to_spawn_timeline_definition(const LevelDefinition &level_definition) {
    SpawnTimelineDefinition result;
    result.waves.reserve(level_definition.waves.size());
    for (const auto &wave_definition : level_definition.waves) {
        SpawnTimelineWave wave;
        wave.wave_number = wave_definition.wave_number;
        wave.spawns.reserve(wave_definition.spawns.size());
        for (const auto &spawn_definition : wave_definition.spawns) {
            wave.spawns.push_back({
                .time = spawn_definition.time,
                .type = to_std_string(spawn_definition.type),
            });
        }
        result.waves.push_back(wave);
    }
    return result;
}

} // namespace

bool SpawnScheduler::load_level(const String &path) {
    const auto loaded_level = LevelLoader::load(path);
    if (!loaded_level) {
        return false;
    }

    load_level_definition(*loaded_level);
    return true;
}

void SpawnScheduler::load_level_definition(const LevelDefinition &level_definition) {
    level_definition_ = level_definition;
    timeline_.load(to_spawn_timeline_definition(level_definition));
}

void SpawnScheduler::configure(const UnitCatalog *unit_catalog, const GridQueryService *grid) {
    unit_catalog_ = unit_catalog;
    grid_ = grid;
}

void SpawnScheduler::start() { timeline_.start(); }

void SpawnScheduler::stop() { timeline_.stop(); }

SpawnSchedulerUpdate SpawnScheduler::update(double delta) {
    SpawnSchedulerUpdate update;
    const SpawnTimelineUpdate timeline_update = timeline_.advance(delta);
    update.wave_changed = timeline_update.wave_changed;
    update.all_spawns_completed = timeline_update.all_spawns_completed;

    for (const auto &spawn : timeline_update.due_spawns) {
        if (unit_catalog_ == nullptr || grid_ == nullptr) {
            continue;
        }

        const auto config = unit_catalog_->get_unit(to_godot_string(spawn.type));
        if (!config) {
            continue;
        }

        const real_t spawn_y_pos = grid_->sample_belt_y();
        const real_t spawn_x_pos = grid_->spawn_x();
        update.spawn_requests.push_back({
            .config = *config,
            .position = Vector2(spawn_x_pos, spawn_y_pos),
        });
    }

    return update;
}

} // namespace defn