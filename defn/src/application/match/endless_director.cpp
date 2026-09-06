// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "endless_director.h"

namespace defn {

namespace {

// A run that never passes its budget ceiling still has to terminate somewhere. Nothing reaches this: at any
// escalation above 1.0 the ceiling arrives first, and at 1.0 or below the wall clock does.
constexpr int MAXIMUM_WAVE = 100000;

} // namespace

void EndlessDirector::configure(MatchDirector *director, ProgressionService *progression, const EndlessSchedule &schedule, RandomSource *random) {
    director_ = director;
    progression_ = progression;
    random_ = random;
    generator_.configure(schedule);
    current_wave_ = 0;
    appended_through_wave_ = 0;
    elapsed_seconds_ = 0.0;
    stopped_ = false;
    recorded_ = false;

    final_wave_ = MAXIMUM_WAVE;
    for (int wave = 1; wave <= MAXIMUM_WAVE; ++wave) {
        if (generator_.budget(wave) > generator_.tuning().budget_ceiling) {
            final_wave_ = wave;
            break;
        }
    }
}

void EndlessDirector::seed_first_wave() { top_up(); }

MatchUpdate EndlessDirector::update(double delta) {
    if (director_ == nullptr) {
        return {};
    }

    elapsed_seconds_ += delta;
    top_up();

    // Before the director advances, not after: the run has to be conceded on the tick the final wave would have
    // spawned, or that wave lands and the ending is decided by the fight rather than by the schedule.
    if (!stopped_ && should_stop()) {
        stopped_ = true;
        return director_->concede_match();
    }

    MatchUpdate result = director_->update(delta);
    if (result.wave_changed.has_value()) {
        current_wave_ = result.wave_changed->current_wave;
        // Income compounds with difficulty unless something pushes back, and the push-back is per wave rather than
        // per kill so that it is legible in the same units the sweep tunes.
        director_->set_bounty_scale(generator_.bounty_multiplier(current_wave_));
        director_->award_survival_bonus(generator_.tuning().survival_bonus_per_wave);
    }

    return result;
}

void EndlessDirector::finalize_ended_run(MatchUpdate &update) {
    if (!update.match_ended.has_value() || recorded_) {
        return;
    }

    recorded_ = true;
    MatchSummaryModel &summary = update.match_ended->summary_model;
    summary.endless.run = true;
    if (progression_ == nullptr) {
        return;
    }

    const EndlessRunRecordResult recorded = progression_->record_endless_run(summary.wave_reached, summary.level_score);
    summary.endless.best_wave = recorded.record.best_wave;
    summary.endless.best_score = recorded.record.best_score;
    summary.endless.record_wave = recorded.record_wave;
    summary.endless.record_score = recorded.record_score;
    summary.endless.available = true;
}

void EndlessDirector::top_up() {
    if (stopped_ || director_ == nullptr || random_ == nullptr) {
        return;
    }

    // The invariant is that the highest appended wave has not started spawning yet. While it holds there is always a
    // spawn ahead of the cursor, so `all_spawns_spawned()` is never true and the match cannot end by victory -- not
    // even in the gap between one wave's last spawn and the next wave's first.
    //
    // That is why the final wave is appended rather than withheld: withholding it drains the timeline, and a drained
    // timeline with a clear field is exactly the victory condition.
    while (appended_through_wave_ < final_wave_ && (appended_through_wave_ == 0 || generator_.wave_start_time(appended_through_wave_) <= elapsed_seconds_)) {
        const int next_wave = appended_through_wave_ + 1;
        director_->append_wave(generator_.generate(next_wave, *random_));
        appended_through_wave_ = next_wave;
    }
}

bool EndlessDirector::should_stop() const {
    if (elapsed_seconds_ >= generator_.tuning().wall_clock_ceiling) {
        return true;
    }

    // The final wave is the first one the budget ceiling declares unwinnable by construction. Reaching its slot is
    // as far as the schedule goes, so the run ends there rather than on a wave nobody measured.
    return appended_through_wave_ >= final_wave_ && elapsed_seconds_ >= generator_.wave_start_time(final_wave_);
}

} // namespace defn
