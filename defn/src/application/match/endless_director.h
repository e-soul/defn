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
class EndlessDirector {
  public:
    // No pointer is owned; all must outlive this.
    void configure(MatchDirector *director, ProgressionService *progression, const EndlessSchedule &schedule, RandomSource *random);

    // Seeds the timeline with wave 1. Must be called after the director has loaded its level definition -- loading
    // clears the timeline -- and before the match begins, or the timeline starts empty and completes at once.
    void seed_first_wave();

    // Advances the run by `delta`, topping the timeline up first so the director never sees the end of it.
    MatchUpdate update(double delta);

    // Writes the run into the profile and fills in the endless half of the summary, once, whichever path ended the
    // match -- the base falling, the budget ceiling or the wall clock all arrive here.
    void finalize_ended_run(MatchUpdate &update);

    [[nodiscard]] int current_wave() const { return current_wave_; }
    [[nodiscard]] int appended_through_wave() const { return appended_through_wave_; }
    [[nodiscard]] double elapsed_seconds() const { return elapsed_seconds_; }

  private:
    void top_up();
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
    bool stopped_ = false;
    bool recorded_ = false;
};

} // namespace defn

#endif
