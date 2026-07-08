#ifndef SPAWN_SCHEDULER_H
#define SPAWN_SCHEDULER_H

#include "level_definition.h"
#include "match_outputs.h"
#include "random_source.h"
#include "runtime_service_interfaces.h"
#include "spawn_timeline.h"

#include <godot_cpp/variant/string.hpp>

#include <optional>
#include <vector>

namespace defn {

using namespace godot;

class UnitCatalog;

struct SpawnSchedulerUpdate {
    std::vector<SpawnUnitIntent> spawn_unit_intents;
    std::optional<WaveChanged> wave_changed;
    bool all_spawns_completed = false;
};

class SpawnScheduler {
  public:
    void load_level_definition(const LevelDefinition &level_definition);
    void configure(const UnitCatalog *unit_catalog, const GridQueryService *grid, RandomSource *random);
    void start();
    void stop();
    SpawnSchedulerUpdate update(double delta);

    bool is_running() const { return timeline_.is_running(); }
    bool all_waves_spawned() const { return timeline_.all_spawns_spawned(); }
    int get_total_waves() const { return level_definition_ ? static_cast<int>(level_definition_->waves.size()) : 0; }
    int get_level_number() const { return level_definition_ ? level_definition_->level_id : 0; }
    String get_level_name() const { return level_definition_ ? String(level_definition_->name.c_str()) : String(); }
    int get_starting_core_resource() const { return level_definition_ ? level_definition_->starting_core_resource : 100; }
    int get_base_integrity() const { return level_definition_ ? level_definition_->base_integrity : 3; }
    String get_background_path() const { return level_definition_ ? String(level_definition_->background_path.c_str()) : String(); }

  private:
    std::optional<LevelDefinition> level_definition_;
    SpawnTimeline timeline_;
    const UnitCatalog *unit_catalog_ = nullptr;
    const GridQueryService *grid_ = nullptr;
    RandomSource *random_ = nullptr;
};

} // namespace defn

#endif