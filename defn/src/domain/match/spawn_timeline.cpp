// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "spawn_timeline.h"

#include <algorithm>
#include <cstddef>
#include <iterator>

namespace defn {

void SpawnTimeline::load(const SpawnTimelineDefinition &definition) {
    all_spawns_.clear();
    level_timer_ = 0.0;
    current_wave_ = 0;
    next_spawn_idx_ = 0;
    running_ = false;
    completion_emitted_ = false;

    for (const auto &wave : definition.waves) {
        for (const auto &spawn : wave.spawns) {
            all_spawns_.push_back({
                .time = spawn.time,
                .type = spawn.type,
                .wave = wave.wave_number,
                .scale = spawn.scale.composed_with(wave.scale),
            });
        }
    }

    std::ranges::sort(all_spawns_, [](const FlatSpawn &left, const FlatSpawn &right) { return left.time < right.time; });
}

void SpawnTimeline::append(const SpawnTimelineWave &wave) {
    for (const auto &spawn : wave.spawns) {
        all_spawns_.push_back({
            .time = spawn.time,
            .type = spawn.type,
            .wave = wave.wave_number,
            .scale = spawn.scale.composed_with(wave.scale),
        });
    }

    // Only the spawns still ahead of the cursor are ordered. Sorting the whole list would reorder spawns already
    // handed out, and the cursor is an index into it.
    std::sort(all_spawns_.begin() + static_cast<std::ptrdiff_t>(std::min(next_spawn_idx_, all_spawns_.size())), all_spawns_.end(),
              [](const FlatSpawn &left, const FlatSpawn &right) { return left.time < right.time; });
}

void SpawnTimeline::start() {
    level_timer_ = 0.0;
    next_spawn_idx_ = 0;
    current_wave_ = 0;
    completion_emitted_ = false;
    running_ = true;
}

void SpawnTimeline::stop() { running_ = false; }

SpawnTimelineUpdate SpawnTimeline::advance(double delta) {
    SpawnTimelineUpdate update;
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

        update.due_spawns.push_back({
            .type = spawn.type,
            .wave = spawn.wave,
            .scale = spawn.scale,
        });
        ++next_spawn_idx_;
    }

    if (!completion_emitted_ && all_spawns_spawned()) {
        completion_emitted_ = true;
        update.all_spawns_completed = true;
    }

    return update;
}

} // namespace defn