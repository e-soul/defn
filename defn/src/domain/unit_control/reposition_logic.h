// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef REPOSITION_LOGIC_H
#define REPOSITION_LOGIC_H

namespace defn {

inline constexpr float REPOSITION_ARRIVAL_EPSILON = 0.01F;

enum class UnitControlMode { AUTOMATIC, REPOSITIONING };

struct RepositionState {
    UnitControlMode mode = UnitControlMode::AUTOMATIC;
    float destination_x = 0.0F;
};

struct RepositionIntents {
    bool face_backward = false;
    bool walk = false;
    bool suspend_combat = false;
    bool face_forward = false;
    bool resume_combat = false;
};

struct RepositionRequest {
    RepositionState state;
    RepositionIntents intents;
    bool accepted = false;
};

struct RepositionStep {
    RepositionState state;
    RepositionIntents intents;
    float next_x = 0.0F;
    bool arrived = false;
};

[[nodiscard]] RepositionRequest request_reposition(const RepositionState &state, float current_x, float destination_x);
[[nodiscard]] RepositionStep advance_reposition(const RepositionState &state, float current_x, float speed_pixels_per_second, double delta,
                                                float arrival_epsilon = REPOSITION_ARRIVAL_EPSILON);
[[nodiscard]] RepositionState cancel_reposition(const RepositionState &state);

} // namespace defn

#endif
