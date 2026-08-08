// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UNIT_CONTROL_CONFIG_H
#define UNIT_CONTROL_CONFIG_H

#include "content_values.h"

namespace defn {

struct GroundMarkerConfig {
    float radius_x = 26.0F;
    float radius_y = 8.0F;
    float border_width = 2.0F;
    float ground_offset_y = 8.0F;
    Color fill_color = {0.12F, 0.85F, 0.48F, 0.24F};
    Color border_color = {0.75F, 1.0F, 0.86F, 0.95F};
};

struct DestinationMarkerConfig {
    float radius_x = 14.0F;
    float radius_y = 4.0F;
    float border_width = 1.5F;
    float minimum_scale = 0.65F;
    float maximum_scale = 1.15F;
    double pulse_duration_seconds = 0.28;
    int pulse_count = 3;
    Color fill_color = {0.2F, 0.9F, 0.58F, 0.18F};
    Color border_color = {0.8F, 1.0F, 0.88F, 0.9F};
};

struct UnitPickingConfig {
    float fallback_radius = 12.0F;
    int max_candidates = 64;
};

struct RepositionConfig {
    float arrival_epsilon = 0.01F;
};

struct UnitControlConfig {
    UnitPickingConfig picking{};
    GroundMarkerConfig selection_marker{};
    GroundMarkerConfig hover_marker = {
        .fill_color = {0.12F, 0.85F, 0.48F, 0.072F},
        .border_color = {0.75F, 1.0F, 0.86F, 0.475F},
    };
    DestinationMarkerConfig destination_marker{};
    RepositionConfig reposition{};
};

} // namespace defn

#endif
