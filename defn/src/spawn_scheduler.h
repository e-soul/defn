#ifndef SPAWN_SCHEDULER_H
#define SPAWN_SCHEDULER_H

#include "level_definition.h"

#include <godot_cpp/variant/string.hpp>

#include <optional>
#include <vector>

namespace defn {

using namespace godot;

class GridManager;
class Unit;
class UnitDataLoader;

struct SpawnSchedulerUpdate {
    std::vector<Unit *> spawned_enemies;
    std::optional<int> wave_changed;
    bool all_spawns_completed = false;
};

class SpawnScheduler {
  public:
    bool load_level(const String &path);
    void configure(const UnitDataLoader *unit_data, GridManager *grid);
    void start();
    void stop();
    SpawnSchedulerUpdate update(double delta);

    bool is_running() const { return running_; }
    bool all_waves_spawned() const { return next_spawn_idx_ >= all_spawns_.size(); }
    int get_total_waves() const { return level_definition_ ? static_cast<int>(level_definition_->waves.size()) : 0; }
    int get_starting_core_resource() const { return level_definition_ ? level_definition_->starting_core_resource : 100; }
    int get_base_integrity() const { return level_definition_ ? level_definition_->base_integrity : 3; }
    String get_background_path() const { return level_definition_ ? level_definition_->background_path : String(); }

  private:
    struct FlatSpawn {
        double time = 0.0;
        String type;
        int wave = 0;
    };

    std::optional<LevelDefinition> level_definition_;
    std::vector<FlatSpawn> all_spawns_;
    const UnitDataLoader *unit_data_ = nullptr;
    GridManager *grid_ = nullptr;
    double level_timer_ = 0.0;
    int current_wave_ = 0;
    bool running_ = false;
    bool completion_emitted_ = false;
    size_t next_spawn_idx_ = 0;
};

} // namespace defn

#endif