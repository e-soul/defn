// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "reposition_logic.h"

#include <algorithm>
#include <cmath>

namespace defn {

RepositionRequest request_reposition(const RepositionState &state, float current_x, float destination_x) {
    if (destination_x >= current_x) {
        return {.state = state};
    }

    return {
        .state = {.mode = UnitControlMode::REPOSITIONING, .destination_x = destination_x},
        .intents = {.face_backward = true, .walk = true, .suspend_combat = state.mode == UnitControlMode::AUTOMATIC},
        .accepted = true,
    };
}

RepositionStep advance_reposition(const RepositionState &state, float current_x, float speed_pixels_per_second, double delta, float arrival_epsilon) {
    RepositionStep step{.state = state, .next_x = current_x};
    if (state.mode != UnitControlMode::REPOSITIONING) {
        return step;
    }

    const float epsilon = std::max(arrival_epsilon, 0.0F);
    const float remaining = current_x - state.destination_x;
    if (remaining <= epsilon) {
        step.state = {.mode = UnitControlMode::AUTOMATIC, .destination_x = state.destination_x};
        step.intents = {.face_forward = true, .resume_combat = true};
        step.next_x = state.destination_x;
        step.arrived = true;
        return step;
    }

    if (speed_pixels_per_second <= 0.0F || delta <= 0.0) {
        step.intents = {.face_backward = true, .walk = true, .suspend_combat = true};
        return step;
    }

    const float displacement = speed_pixels_per_second * static_cast<float>(delta);
    step.next_x = std::max(current_x - displacement, state.destination_x);
    step.intents = {.face_backward = true, .walk = true, .suspend_combat = true};
    if (std::abs(step.next_x - state.destination_x) <= epsilon) {
        step.state = {.mode = UnitControlMode::AUTOMATIC, .destination_x = state.destination_x};
        step.intents = {.face_forward = true, .resume_combat = true};
        step.next_x = state.destination_x;
        step.arrived = true;
    }
    return step;
}

RepositionState cancel_reposition(const RepositionState &state) { return {.mode = UnitControlMode::AUTOMATIC, .destination_x = state.destination_x}; }

} // namespace defn
