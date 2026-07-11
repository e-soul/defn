#include "spawn_scheduler.h"

#include "unit_definition.h"
#include "unit_runtime_config_resolver.h"

namespace defn {

namespace {

MatchUnitSide to_match_unit_side(UnitSide side) { return side == UnitSide::FRIENDLY ? MatchUnitSide::Friendly : MatchUnitSide::Hostile; }

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
                .type = spawn_definition.type,
            });
        }
        result.waves.push_back(wave);
    }
    return result;
}

} // namespace

void SpawnScheduler::load_level_definition(const LevelDefinition &level_definition) {
    level_definition_ = level_definition;
    timeline_.load(to_spawn_timeline_definition(level_definition));
}

void SpawnScheduler::configure(const UnitCatalog *unit_catalog, const GridQueryService *grid, RandomSource *random) {
    unit_catalog_ = unit_catalog;
    grid_ = grid;
    random_ = random;
}

void SpawnScheduler::start() { timeline_.start(); }

void SpawnScheduler::stop() { timeline_.stop(); }

SpawnSchedulerUpdate SpawnScheduler::update(double delta) {
    SpawnSchedulerUpdate update;
    const SpawnTimelineUpdate timeline_update = timeline_.advance(delta);
    if (timeline_update.wave_changed.has_value()) {
        update.wave_changed = WaveChanged{.current_wave = *timeline_update.wave_changed, .total_waves = get_total_waves()};
    }
    update.all_spawns_completed = timeline_update.all_spawns_completed;

    for (const auto &spawn : timeline_update.due_spawns) {
        if (unit_catalog_ == nullptr || grid_ == nullptr || random_ == nullptr) {
            continue;
        }

        const auto config = unit_catalog_->get_unit(spawn.type);
        if (!config) {
            continue;
        }

        const double spawn_y_pos = grid_->sample_belt_y();
        const double spawn_x_pos = grid_->spawn_x();
        update.spawn_unit_intents.push_back({
            .unit_id = spawn.type,
            .side = to_match_unit_side(config->side),
            .position = {.x = spawn_x_pos, .y = spawn_y_pos},
            .runtime_profile = UnitRuntimeProfile::from_unit_config(*config),
            .resolved_runtime_config = resolve_unit_runtime_config(to_runtime_range_config(*config), *random_),
        });
    }

    return update;
}

} // namespace defn
