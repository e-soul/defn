// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SPAWN_TIMELINE_H
#define SPAWN_TIMELINE_H

#include "hostile_scaling.h"

#include <optional>
#include <string>
#include <vector>

namespace defn {

struct SpawnTimelineSpawn {
    double time = 0.0;
    std::string type;
    HostileScale scale;
};

struct SpawnTimelineWave {
    int wave_number = 0;
    std::vector<SpawnTimelineSpawn> spawns;
    HostileScale scale;
};

struct SpawnTimelineDefinition {
    std::vector<SpawnTimelineWave> waves;
};

struct DueSpawn {
    std::string type;
    int wave = 0;
    // The wave's scale and the body's own, already composed: a caller spawning this never has to know there were two.
    HostileScale scale;
};

struct SpawnTimelineUpdate {
    std::vector<DueSpawn> due_spawns;
    std::optional<int> wave_changed;
    bool all_spawns_completed = false;
};

class SpawnTimeline {
  public:
    void load(const SpawnTimelineDefinition &definition);

    // Adds a wave to a timeline that may already be running, with its times already absolute. Nothing about the
    // clock, the cursor or the completion latch is reset: a run that keeps appending simply never reaches the end
    // of the list, which is how endless mode avoids `all_spawns_completed` without the timeline knowing it exists.
    void append(const SpawnTimelineWave &wave);

    void start();
    void stop();
    [[nodiscard]] SpawnTimelineUpdate advance(double delta);

    // Brings the next spawn that has not been handed out to `lead` seconds away, by moving the clock forward, and
    // returns how much time was skipped. Zero when nothing is pending, when the timeline is stopped, or when the
    // wait is already `lead` or shorter.
    //
    // The clock only ever moves forward here, so this cannot delay anything, and a gap already tighter than `lead`
    // is left exactly as authored -- the stagger inside a wave is not a gap this is meant to close. The timeline
    // has no opinion about *why* a caller wants the gap closed; it only knows how to close one.
    [[nodiscard]] double pull_next_spawn_forward(double lead);

    [[nodiscard]] bool is_running() const { return running_; }
    [[nodiscard]] bool all_spawns_spawned() const { return next_spawn_idx_ >= all_spawns_.size(); }

  private:
    struct FlatSpawn {
        double time = 0.0;
        std::string type;
        int wave = 0;
        HostileScale scale;
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