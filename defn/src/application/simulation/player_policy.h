// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef PLAYER_POLICY_H
#define PLAYER_POLICY_H

#include "sim_entity.h"
#include "unit_definition.h"

#include <span>
#include <string>
#include <vector>

namespace defn {

// What a player can actually do during a match. Deployment is the whole vocabulary: the camera is pushed by units
// crossing trigger strips, never by the player, and manual repositioning arrives with the play harness.
struct PlayerCommand {
    enum class Kind { NOOP, DEPLOY };

    Kind kind = Kind::NOOP;
    std::string unit_id;

    static PlayerCommand deploy(std::string unit_id) { return {.kind = Kind::DEPLOY, .unit_id = std::move(unit_id)}; }
};

// Everything a policy is allowed to see. Deliberately the same facts a player has on screen: the HUD numbers, the
// roster, and what is currently on the belt.
struct MatchObservation {
    double elapsed_seconds = 0.0;
    int energy = 0;
    int base_health = 0;
    int base_max_health = 0;
    int current_wave = 0;
    int total_waves = 0;
    // Where the base stands, and where a deployment would land: the two ends of the ground worth defending.
    float base_position_x = 0.0F;
    float deploy_x = 0.0F;
    // How far out the base itself can shoot. Anything inside this line is being fought by the base as well, and is
    // also a short walk from the deploy point -- which is what makes holding ground near the base cheap.
    float base_engage_x = 0.0F;
    std::span<const SimEntity> entities;
    // The unlocked roster, with progression stat effects already applied.
    std::span<const UnitConfig> roster;
};

// A balance sim without a player model is a sim that lies. Ship several, and report the spread rather than a number.
class PlayerPolicy {
  public:
    virtual ~PlayerPolicy() = default;

    [[nodiscard]] virtual const char *name() const = 0;
    virtual std::vector<PlayerCommand> decide(const MatchObservation &observation) = 0;
};

} // namespace defn

#endif
