// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef DEFN_BALANCE_RUNNER_H
#define DEFN_BALANCE_RUNNER_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace defn {

using namespace godot;

// Measures the two roster numbers BALANCE.md has so far had to estimate: what one hostile costs the player, and what
// one friendly buys for its energy.
//
// Both are answered by putting the unit under test against a fixed reference force in the combat lab and averaging
// over seeds, so the attack-range variation each spawn draws is smoothed out rather than pinned away. The reference
// never changes between units, which is what makes the columns comparable.
class DefnBalanceRunner : public RefCounted {
    GDCLASS(DefnBalanceRunner, RefCounted);

  public:
    // args: {seeds: int, out: String path}. Returns {success, threat, roster} and prints both tables.
    static Dictionary measure(const Dictionary &args);

  protected:
    static void _bind_methods();
};

} // namespace defn

#endif
