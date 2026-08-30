// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef DEFN_SIM_RUNNER_H
#define DEFN_SIM_RUNNER_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace defn {

using namespace godot;

// The Godot-facing entry point for a sweep, in the mould of `DefnHostedTestRunner`.
//
// The kernel is pure C++ and cannot read the shipped JSON; the loaders that can are Godot-bound. So this runs inside a
// headless Godot process, loads content through the very same loaders the game uses, hands the resulting plain structs
// to the kernel, and writes one JSON line per run. Engine startup is paid once; the matches themselves run at native
// speed with no nodes, physics, assets or rendering involved.
class DefnSimRunner : public RefCounted {
    GDCLASS(DefnSimRunner, RefCounted);

  public:
    // args: {scenario: String path, seeds: int, out: String path}. Returns {success, runs, victories, out, failures}.
    static Dictionary run_sweep(const Dictionary &args);

    // The same scenario, measured as a *critical purse* rather than a win rate.
    //
    // Win rate saturates: at a generous purse every composition wins and at a mean one none does, so a table read at
    // one fixed purse is mostly zeroes and hundreds and cannot rank anything. This bisects
    // `starting_core_resource` per (engagement, composition) for the smallest purse that wins half the time --
    // exactly what `critical_budget` does for `scons matrix`, except that here the energy arrives over time, so the
    // answer prices tempo as well as composition.
    //
    // args: {scenario, seeds, out, max_purse, tolerance, max_iterations, win_threshold}.
    static Dictionary run_purse_bisection(const Dictionary &args);

  protected:
    static void _bind_methods();
};

} // namespace defn

#endif
