// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef DEFN_UI_LAYOUT_H
#define DEFN_UI_LAYOUT_H

#include <algorithm>

namespace defn {
inline constexpr float UI_COMPACT_WIDTH = 1400.0F;
struct UiLayout {
    float width;
    float height;
    float scale;
    bool compact;
};

// Display dimensions are CSS pixels on web. Density never changes this decision.
inline UiLayout fit_ui(float width, float height) {
    const float fit = std::max(1.0F, std::min({width, height * (1920.0F / 1080.0F), 1920.0F}));
    return {.width = fit, .height = fit * (1080.0F / 1920.0F), .scale = 1920.0F / fit, .compact = fit < UI_COMPACT_WIDTH};
}
} // namespace defn
#endif
