// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef ENDLESS_DIRECTOR_H
#define ENDLESS_DIRECTOR_H

#include "endless_wave_generator.h"
#include "match_director.h"
#include "match_outputs.h"
#include "progression_service.h"
#include "random_source.h"

namespace defn {

// Keeps an endless run fed. It owns the generator and drives a `MatchDirector` that knows nothing about the mode:
// every wave is appended one wave ahead of the spawn cursor, so the timeline is never observed as complete and the
// victory condition is avoided by staying ahead of it rather than by a flag.
//
// The run ends the same way a campaign level does -- through `MatchDirector`'s own end path -- either because the
// base fell, or because the escalation passed the budget the schedule declares unwinnable, or because the run hit
// the wall-clock ceiling.
//
// It also owns the run's pacing: the schedule authors the spacing between waves for a player who is still fighting,
// and this closes it for one who is not. That makes the run's clock a schedule clock rather than a wall clock --
// see `schedule_seconds`.
class EndlessDirector {
  public:
    // No pointer is owned; all must outlive this.
    void configure(MatchDirector *director, ProgressionService *progression, const EndlessSchedule &schedule, RandomSource *random);

    // Seeds the timeline with wave 1. Must be called after the director has loaded its level definition -- loading
    // clears the timeline -- and before the match begins, or the timeline starts empty and completes at once.
    void seed_first_wave();

    // Applies wave 1's rules to a match that has just begun. Must be called *after* `MatchDirector::begin_match`,
    // because the session learns its ceilings there and ignores a rule set before it has them.
    //
    // Without this the opening frames run under the level's raw numbers rather than the schedule's: the supply
    // allowance reads as the level's ceiling until the first wave changes it, so the HUD claims room the player
    // does not have and every deploy card looks available.
    void begin_run();

    // Advances the run by `delta`, topping the timeline up first so the director never sees the end of it.
    MatchUpdate update(double delta);

    // Writes the run into the profile and fills in the endless half of the summary, once, whichever path ended the
    // match -- the base falling, the budget ceiling or the wall clock all arrive here.
    void finalize_ended_run(MatchUpdate &update);

    [[nodiscard]] int current_wave() const { return current_wave_; }
    [[nodiscard]] int appended_through_wave() const { return appended_through_wave_; }

    // Seconds the run has actually been played. The wall-clock ceiling is measured against this.
    [[nodiscard]] double elapsed_seconds() const { return elapsed_seconds_; }

    // Where the schedule's clock stands: the seconds played plus every idle gap the cleared-field rule closed. Wave
    // start times, the budget ceiling and the spawn timeline all read this one, so a run that clears fast arrives at
    // the same wave against the same budget in less real time.
    [[nodiscard]] double schedule_seconds() const { return elapsed_seconds_ + skipped_seconds_; }

  private:
    // The per-wave counter-pressures, in one place so the opening and every wave after it cannot drift apart.
    void apply_wave_rules(int wave);
    void top_up();
    // Closes the dead air between a wave the player has already cleared and the next arrival.
    void close_cleared_gap();
    [[nodiscard]] bool should_stop() const;

    MatchDirector *director_ = nullptr;
    ProgressionService *progression_ = nullptr;
    RandomSource *random_ = nullptr;
    EndlessWaveGenerator generator_;
    int current_wave_ = 0;
    int appended_through_wave_ = 0;
    // The first wave the budget ceiling puts out of reach. It is generated and appended so the timeline never
    // drains, and the run is conceded on the tick it would have opened.
    int final_wave_ = 0;
    double elapsed_seconds_ = 0.0;
    // Schedule time skipped by `close_cleared_gap`, kept apart from the seconds played so the wall-clock ceiling
    // stays a reading of how long the player has been at it.
    double skipped_seconds_ = 0.0;
    bool stopped_ = false;
    bool recorded_ = false;
};

} // namespace defn

#endif
