// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef DEFN_ENDLESS_RUNNER_H
#define DEFN_ENDLESS_RUNNER_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace defn {

using namespace godot;

// Sweeps the endless mode's three tuning knobs, in the mould of `DefnSimRunner`.
//
// The mode's quality is entirely a question of whether the escalation `r`, the bounty decay `d` and the drift
// schedule are right, and none of the three can be judged by playing a handful of runs. This runs the real
// `EndlessDirector` against the real `MatchDirector` headlessly across seeds and policies, and writes one JSON line
// per run for `scripts/analyze_endless.py` to read three things out of:
//
//   1. Run length -- the wave-reached distribution for a competent policy.
//   2. Not solved -- no single friendly mix reaching the target wave across all seeds. This is the ship gate.
//   3. Economy -- energy held at each wave staying bounded rather than trending up.
class DefnEndlessRunner : public RefCounted {
    GDCLASS(DefnEndlessRunner, RefCounted);

  public:
    // args: {seeds: int, out: String path, max_seconds: float, base_budget: Array<float>, escalation: Array<float>, bounty_decay: Array<float>,
    //        policies: Array of {kind, label, weights, energy_reserve}}. Every array defaults to a sensible sweep:
    //        the shipped values for the knobs, and the four standard policies plus one mono mix per friendly unit --
    //        the mono mixes are what the not-solved gate is read from.
    // Returns {success, runs, out, max_wave}.
    static Dictionary run_sweep(const Dictionary &args);

  protected:
    static void _bind_methods();
};

} // namespace defn

#endif
