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

  protected:
    static void _bind_methods();
};

} // namespace defn

#endif
