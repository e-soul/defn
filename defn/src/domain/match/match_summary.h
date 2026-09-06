// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef MATCH_SUMMARY_H
#define MATCH_SUMMARY_H

#include <string>
#include <vector>

namespace defn {

// The endless half of a match summary. Carried on every summary rather than on an endless-only one, because the
// campaign score screen is where the mode is announced and where its button lives.
struct MatchEndlessSummary {
    // Edge-triggered on the run that first opened the mode, so replaying the gate level announces nothing.
    bool unlocked = false;
    // Whether the mode can be entered at all, which is what the button is gated on.
    bool available = false;
    // Whether *this* match was an endless run, which turns the score screen into a run-over screen.
    bool run = false;
    int best_wave = 0;
    int best_score = 0;
    bool record_wave = false;
    bool record_score = false;
};

struct MatchSummaryModel {
    bool victory = false;
    int enemies_killed = 0;
    int kill_score = 0;
    int hearts_remaining = 0;
    int hearts_total = 0;
    int integrity_bonus = 0;
    int survival_bonus = 0;
    int completion_bonus = 0;
    int level_score = 0;
    int new_total_score = 0;
    int wave_reached = 0;
    std::string current_level_id;
    std::string next_level_id;
    std::vector<std::string> new_unlocks;
    MatchEndlessSummary endless;
};

} // namespace defn

#endif