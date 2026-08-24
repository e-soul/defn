// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UNIT_ANIMATION_STATE_H
#define UNIT_ANIMATION_STATE_H

#include "animation_clock.h"
#include "combat_logic.h"
#include "unit_definition.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace defn {

enum class UnitPose { WALK, ATTACK, SHOOT, DEATH };

// What a caller may have to react to after a state call. Everything else it can read back from the state itself.
struct UnitAnimationUpdate {
    bool shoot_effect_fired = false;
};

// The engine-free half of a unit's animation: which animation is current, how far its clock has run, and the moment a
// shot leaves the muzzle. Combat reads its timing directly. `AnimationController` wraps it and mirrors the result onto
// a sprite; the simulator drives it without Godot at all. One implementation, so the two cannot disagree.
class UnitAnimationState {
  public:
    void configure(std::vector<std::pair<std::string, AnimConfig>> animations);

    [[nodiscard]] UnitPose get_pose() const { return pose_; }
    [[nodiscard]] const std::string &get_current_animation() const { return current_animation_; }
    [[nodiscard]] const AnimationClock &get_clock() const { return clock_; }
    [[nodiscard]] const AnimConfig *find_animation(std::string_view name) const;

    void set_pose(UnitPose pose);
    void hold_pose(UnitPose pose);
    void play_attack();
    // effect_frame is the animation frame the shot is released on; frame 0 releases it immediately.
    UnitAnimationUpdate play_shoot(int effect_frame);
    bool play_named(std::string_view name, bool restart);
    void cancel_pending_attack();

    UnitAnimationUpdate advance(double delta);

    bool consume_shoot_effect_triggered();
    [[nodiscard]] bool is_attack_animation_playing() const;
    [[nodiscard]] bool is_attack_windup_active() const;

  private:
    enum class Start { RESUME, RESTART, HOLD };

    void apply_animation(std::string_view name, Start start);
    UnitAnimationUpdate update_shoot_effect();

    std::vector<std::pair<std::string, AnimConfig>> animations_;
    std::string current_animation_;
    AnimationClock clock_;
    UnitPose pose_ = UnitPose::WALK;
    bool shoot_effect_pending_ = false;
    bool shoot_effect_ready_ = false;
    int shoot_effect_frame_ = 0;
};

[[nodiscard]] CombatPoseState to_combat_pose_state(UnitPose pose);

} // namespace defn

#endif
