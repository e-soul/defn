// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "reposition_logic.h"

namespace defn {

DEFN_TEST(reposition_accepts_only_destinations_strictly_behind) {
    const RepositionState automatic;
    const RepositionRequest accepted = request_reposition(automatic, 100.0F, 75.0F);
    DEFN_CHECK(accepted.accepted);
    DEFN_CHECK_EQ(accepted.state.mode, UnitControlMode::REPOSITIONING);
    DEFN_CHECK_CLOSE(accepted.state.destination_x, 75.0, 0.001);
    DEFN_CHECK(accepted.intents.face_backward);
    DEFN_CHECK(accepted.intents.walk);
    DEFN_CHECK(accepted.intents.suspend_combat);

    DEFN_CHECK(!request_reposition(automatic, 100.0F, 100.0F).accepted);
    DEFN_CHECK(!request_reposition(automatic, 100.0F, 125.0F).accepted);
}

DEFN_TEST(reposition_advances_without_overshoot_and_preserves_external_y) {
    const RepositionState repositioning{.mode = UnitControlMode::REPOSITIONING, .destination_x = 40.0F};
    const float unchanged_y = 213.0F;
    const RepositionStep step = advance_reposition(repositioning, 100.0F, 80.0F, 2.0);

    DEFN_CHECK(step.arrived);
    DEFN_CHECK_CLOSE(step.next_x, 40.0, 0.001);
    DEFN_CHECK_CLOSE(unchanged_y, 213.0, 0.001);
    DEFN_CHECK_EQ(step.state.mode, UnitControlMode::AUTOMATIC);
    DEFN_CHECK(step.intents.face_forward);
    DEFN_CHECK(step.intents.resume_combat);
}

DEFN_TEST(reposition_walks_backward_until_arrival_epsilon) {
    const RepositionState repositioning{.mode = UnitControlMode::REPOSITIONING, .destination_x = 40.0F};
    const RepositionStep walking = advance_reposition(repositioning, 100.0F, 20.0F, 0.5);
    DEFN_CHECK(!walking.arrived);
    DEFN_CHECK_CLOSE(walking.next_x, 90.0, 0.001);
    DEFN_CHECK(walking.intents.face_backward);
    DEFN_CHECK(walking.intents.walk);
    DEFN_CHECK(walking.intents.suspend_combat);

    const RepositionStep arrived = advance_reposition(repositioning, 40.005F, 20.0F, 0.0);
    DEFN_CHECK(arrived.arrived);
    DEFN_CHECK_CLOSE(arrived.next_x, 40.0, 0.001);
}

DEFN_TEST(reposition_replaces_an_active_destination_without_resuspending_combat) {
    const RepositionState active{.mode = UnitControlMode::REPOSITIONING, .destination_x = 70.0F};
    const RepositionRequest replacement = request_reposition(active, 85.0F, 25.0F);

    DEFN_CHECK(replacement.accepted);
    DEFN_CHECK_CLOSE(replacement.state.destination_x, 25.0, 0.001);
    DEFN_CHECK(!replacement.intents.suspend_combat);
}

DEFN_TEST(reposition_cancellation_does_not_request_combat_resume) {
    const RepositionState active{.mode = UnitControlMode::REPOSITIONING, .destination_x = 25.0F};
    const RepositionState canceled = cancel_reposition(active);
    DEFN_CHECK_EQ(canceled.mode, UnitControlMode::AUTOMATIC);
}

} // namespace defn
