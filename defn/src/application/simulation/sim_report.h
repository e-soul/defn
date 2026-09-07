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
    // How far an endless run got. Zero for a campaign level, where the wave count is authored rather than reached.
    int waves_reached = 0;
    // Energy held when each wave opened. A run whose economy is snowballing shows this trending up; the endless
    // bounty decay is tuned until it does not.
    std::vector<int> energy_at_wave;
    // Friendlies lost during each wave. This is the reading that says whether the *middle* of a run is a fight, and
    // no other number here does: a run can end at the right wave, in the right minutes, at zero integrity, and
    // still be twenty waves of hostiles evaporating against a line that never loses a body. A long run of zeroes
    // here is that run, and it is a difficulty failure the run-length table cannot see.
    std::vector<int> friendly_deaths_at_wave;
    // The wave the line first reached the supply cap, or 0 if it never did. Together with the trace above it dates
    // the moment the player stopped making decisions: at the cap, with nothing dying, there is nothing left to do.
    int first_capped_wave = 0;

    // Wasted economy: the integral of unspent energy over time. High means the player banked what it could have spent.
    double energy_idle_integral = 0.0;
    int peak_concurrent_enemies = 0;
    // The most enemies that appeared inside any five-second window: the spike density levels are tuned against.
    int peak_window_5s = 0;

    int deployments_total = 0;
    // Deployments the policy asked for and the supply cap refused. Zero on an uncapped match, and the direct reading
    // of whether the cap is binding at all -- a cap the player never reaches is a cap that changes nothing.
    //
    // Counted per decision tick rather than per distinct intention: a policy asks every tick, so a line that sits at
    // the cap for a minute registers thousands. Read it as "how much of the run was spent capped", not as a number
    // of deployments the player meant to make.
    int deployments_blocked = 0;
    // The largest the player's line ever got. Against the cap it says how much of the allowance was in use; without
    // one it is the number the mode's arithmetic could not bound.
    int peak_friendlies = 0;
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
