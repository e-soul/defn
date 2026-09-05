// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SIM_POLICIES_H
#define SIM_POLICIES_H

#include "player_policy.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace defn {

struct ScriptedCommand {
    double time_seconds = 0.0;
    std::string unit_id;
};

// Plays a fixed plan: deploy this, then that, at these times. Exact reproduction of one line of play, and the format
// the play harness will replay later. A deployment that cannot be afforded when its moment comes is simply missed,
// the same way it would be for a player who mistimed it.
class ScriptedPolicy final : public PlayerPolicy {
  public:
    explicit ScriptedPolicy(std::vector<ScriptedCommand> script);

    [[nodiscard]] const char *name() const override { return "scripted"; }
    std::vector<PlayerCommand> decide(const MatchObservation &observation) override;

  private:
    std::vector<ScriptedCommand> script_;
    std::size_t next_index_ = 0;
};

// Spends on the best affordable unit the moment energy allows, where "best" is the most expensive one on the roster.
// The floor: no patience, no reading of the belt.
class GreedyPolicy final : public PlayerPolicy {
  public:
    [[nodiscard]] const char *name() const override { return "greedy"; }
    std::vector<PlayerCommand> decide(const MatchObservation &observation) override;
};

// Spends nothing until the leading hostile is inside the base's own weapon range, then commits everything it has
// banked and keeps spending greedily.
//
// This is how the game is actually played, and it is worth two things at once: the base fights alongside the
// deployment, and the walk from the deploy point to the fight is short instead of most of a unit's life. A policy
// that deploys on sight instead throws units at a fight happening far from both.
class DefensivePolicy final : public PlayerPolicy {
  public:
    [[nodiscard]] const char *name() const override { return "defensive"; }
    std::vector<PlayerCommand> decide(const MatchObservation &observation) override;

  private:
    bool committed_ = false;
};

// Holds for expensive units, reacts to what is on the belt, and keeps a reserve. Spends immediately when hostiles
// are close to the base, saves toward the top of the roster otherwise.
//
// It varies *when* to spend, never *what* to buy -- every branch ends in the most expensive affordable unit. The name
// says patience because that is the whole of it; `MixPolicy` is the one that chooses a composition.
class PatiencePolicy final : public PlayerPolicy {
  public:
    explicit PatiencePolicy(int energy_reserve = 15) : energy_reserve_(energy_reserve) {}

    [[nodiscard]] const char *name() const override { return "patience"; }
    std::vector<PlayerCommand> decide(const MatchObservation &observation) override;

  private:
    int energy_reserve_;
};

// Plays a target composition rather than a power ladder: each tick it deploys the affordable unit whose share of the
// field is furthest below its target weight.
//
// Every other policy resolves to "the most expensive thing I can afford", so a sweep of them can only ever compare
// mono-stacks. Nothing about composition is expressible at level scale without this.
class MixPolicy final : public PlayerPolicy {
  public:
    explicit MixPolicy(std::map<std::string, double> weights) : weights_(std::move(weights)) {}

    [[nodiscard]] const char *name() const override { return "mix"; }
    std::vector<PlayerCommand> decide(const MatchObservation &observation) override;

  private:
    std::map<std::string, double> weights_;
};

} // namespace defn

#endif
