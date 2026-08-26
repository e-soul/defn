// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef DEFN_MATRIX_RUNNER_H
#define DEFN_MATRIX_RUNNER_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace defn {

using namespace godot;

// Measures the payoff matrix `M[friendly mix][hostile mix]`, which is the object diversity actually lives in.
//
// Every balance number the project has taken so far is one row or one column of this matrix: the threat table fixes
// the defence, the roster table fixes the attacker, and the campaign sweep only ever compares mono-stacks. None of
// them can see an off-diagonal, so none of them can see whether the right answer changes with the question.
//
// Each cell is a critical budget rather than a win rate, because win rate has saturated at 100% across every advanced
// cell and a saturated scale ranks nothing. Rows are emitted per seed so the analysis can put a confidence interval
// on a cell and refuse to call a difference that is inside seed noise.
class DefnMatrixRunner : public RefCounted {
    GDCLASS(DefnMatrixRunner, RefCounted);

  public:
    // args: {spec: String path to a matrix spec JSON, seeds: int, out: String path}. Writes one JSONL row per
    // (friendly mix, hostile mix, seed) and returns {success, cells, rows, out}.
    static Dictionary measure(const Dictionary &args);

  protected:
    static void _bind_methods();
};

} // namespace defn

#endif
