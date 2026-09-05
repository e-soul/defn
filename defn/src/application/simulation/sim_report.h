// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SIM_REPORT_H
#define SIM_REPORT_H

#include <cstdint>
#include <string>
#include <vector>

namespace defn {

struct SimDeploymentStat {
    std::string unit_id;
    int count = 0;
    int total_energy = 0;
};

struct SimUnitStat {
    std::string unit_id;
    int spawned = 0;
    int damage_dealt = 0;
    int damage_taken = 0;
    int kills = 0;
    int deaths = 0;
    double mean_lifespan_seconds = 0.0;
};

// A hostile that got through and hit the base.
struct SimLeakEvent {
    double time_seconds = 0.0;
    std::string unit_id;
    int damage = 0;
};

// One line of a sweep. Written as JSONL so aggregating a thousand runs is a one-liner.
struct SimMatchReport {
    std::string level_id;
    std::uint32_t seed = 0;
    std::string policy;

    // False when the run hit its time limit with the match still going: neither side had settled it.
    bool decided = false;
    bool victory = false;
    double clear_time_seconds = 0.0;

    int remaining_integrity = 0;
    int base_health = 0;
    int base_max_health = 0;
    int kill_score = 0;
    int level_score = 0;

    // Wasted economy: the integral of unspent energy over time. High means the player banked what it could have spent.
    double energy_idle_integral = 0.0;
    int peak_concurrent_enemies = 0;
    // The most enemies that appeared inside any five-second window: the spike density levels are tuned against.
    int peak_window_5s = 0;

    int deployments_total = 0;
    int energy_spent = 0;
    std::vector<SimDeploymentStat> deployments;
    std::vector<SimUnitStat> per_unit;
    // The x of the leading engagement, sampled once a second.
    std::vector<float> front_line_trace;
    std::vector<SimLeakEvent> leak_events;
    int camera_scroll_events = 0;
};

// One JSON object on one line, no trailing newline.
[[nodiscard]] std::string to_jsonl(const SimMatchReport &report);

} // namespace defn

#endif
