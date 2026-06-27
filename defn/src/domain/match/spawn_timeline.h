#ifndef SPAWN_TIMELINE_H
#define SPAWN_TIMELINE_H

#include <optional>
#include <string>
#include <vector>

namespace defn {

struct SpawnTimelineSpawn {
    double time = 0.0;
    std::string type;
};

struct SpawnTimelineWave {
    int wave_number = 0;
    std::vector<SpawnTimelineSpawn> spawns;
};

struct SpawnTimelineDefinition {
    std::vector<SpawnTimelineWave> waves;
};

struct DueSpawn {
    std::string type;
    int wave = 0;
};

struct SpawnTimelineUpdate {
    std::vector<DueSpawn> due_spawns;
    std::optional<int> wave_changed;
    bool all_spawns_completed = false;
};

class SpawnTimeline {
  public:
    void load(const SpawnTimelineDefinition &definition);
    void start();
    void stop();
    [[nodiscard]] SpawnTimelineUpdate advance(double delta);

    [[nodiscard]] bool is_running() const { return running_; }
    [[nodiscard]] bool all_spawns_spawned() const { return next_spawn_idx_ >= all_spawns_.size(); }

  private:
    struct FlatSpawn {
        double time = 0.0;
        std::string type;
        int wave = 0;
    };

    std::vector<FlatSpawn> all_spawns_;
    double level_timer_ = 0.0;
    int current_wave_ = 0;
    bool running_ = false;
    bool completion_emitted_ = false;
    size_t next_spawn_idx_ = 0;
};

} // namespace defn

#endif