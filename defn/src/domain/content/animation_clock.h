// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef ANIMATION_CLOCK_H
#define ANIMATION_CLOCK_H

#include "unit_definition.h"

#include <algorithm>
#include <cmath>

namespace defn {

// Engine-free model of sprite animation timing. It mirrors AnimatedSprite2D frame stepping: a frame lasts 1/speed
// seconds, a looping animation wraps, and a non-looping one parks on its last frame once that frame has been shown for
// its full duration.
//
// Combat timing depends on it: re-posing, movement cancellation and projectile launch are all gated on how far an
// attack animation has played. Deriving that from this clock rather than from the sprite keeps the rules identical
// with or without a sprite attached, and lets them be exercised without Godot.
class AnimationClock {
  public:
    // Starts config from frame 0.
    void play(const AnimConfig &config) {
        if (!is_usable(config)) {
            return;
        }
        config_ = config;
        has_animation_ = true;
        elapsed_seconds_ = 0.0;
        playing_ = true;
    }

    // Resumes config where it stopped, restarting it if it had already played out. Mirrors AnimatedSprite2D::play on
    // the animation that is already current.
    void resume(const AnimConfig &config) {
        if (!is_usable(config)) {
            return;
        }
        if (!has_animation_ || is_parked_at_end()) {
            play(config);
            return;
        }
        config_ = config;
        playing_ = true;
    }

    // Poses config on frame 0 without running it.
    void hold(const AnimConfig &config) {
        if (!is_usable(config)) {
            return;
        }
        config_ = config;
        has_animation_ = true;
        elapsed_seconds_ = 0.0;
        playing_ = false;
    }

    void stop() { playing_ = false; }

    void advance(double delta) {
        if (!playing_ || !has_animation_ || delta <= 0.0 || config_.speed <= 0.0) {
            return;
        }

        elapsed_seconds_ += delta;
        const double cycle = cycle_seconds();
        if (config_.loop) {
            elapsed_seconds_ = std::fmod(elapsed_seconds_, cycle);
        } else if (elapsed_seconds_ >= cycle) {
            elapsed_seconds_ = cycle;
            playing_ = false;
        }
    }

    [[nodiscard]] bool has_animation() const { return has_animation_; }
    [[nodiscard]] bool is_playing() const { return has_animation_ && playing_; }
    [[nodiscard]] const AnimConfig &config() const { return config_; }
    [[nodiscard]] double elapsed_seconds() const { return elapsed_seconds_; }

    [[nodiscard]] int frame() const {
        if (!has_animation_ || config_.speed <= 0.0) {
            return 0;
        }
        const double index = std::floor(elapsed_seconds_ * config_.speed);
        return std::clamp(static_cast<int>(index), 0, config_.frame_count - 1);
    }

    // True while the animation is still inside the committed frames it may not be interrupted during.
    [[nodiscard]] bool is_windup_active() const { return is_playing() && frame() < config_.windup_frames; }

  private:
    static bool is_usable(const AnimConfig &config) { return config.frame_count > 0; }

    [[nodiscard]] double cycle_seconds() const { return static_cast<double>(config_.frame_count) / config_.speed; }

    [[nodiscard]] bool is_parked_at_end() const { return !playing_ && !config_.loop && config_.speed > 0.0 && elapsed_seconds_ >= cycle_seconds(); }

    AnimConfig config_{};
    double elapsed_seconds_ = 0.0;
    bool playing_ = false;
    bool has_animation_ = false;
};

} // namespace defn

#endif
