// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef HUD_LAYOUT_H
#define HUD_LAYOUT_H

#include <span>
#include <vector>

namespace defn {

/// Displayed browser pixels and engine UI units are deliberately separate. Retina backing pixels never enter
/// this model. Browser chrome outside the canvas (including safe-area padding) is already excluded.
struct UiViewportMetrics {
    float width = 1920.0F;
    float height = 1080.0F;
    float css_width = 1920.0F;
    float css_height = 1080.0F;
    float overlay_width = 0.0F;
    float overlay_height = 0.0F;
    bool web = false;
    bool coarse_pointer = false;

    bool operator==(const UiViewportMetrics &) const = default;
};

/// Small-screen card dimensions in displayed browser pixels, supplied by the UI theme adapter.
struct HudCardLayout {
    float width = 180.0F;
    float height = 44.0F;
    float portrait_size = 28.0F;
    float inset = 5.0F;
};

struct HudSizing {
    bool responsive = false;
    bool compact = false;
    bool wrap_cards = false;
    float pixels_per_unit = 1.0F;
    float readout_scale = 1.0F;
    float margin = 24.0F;
    float gap = 12.0F;
    float level_width = 0.0F;
    float integrity_width = 0.0F;
    float card_width = 190.0F;
    float card_height = 110.0F;
    float portrait_size = 80.0F;
    float card_inset = 8.0F;
    float pause_width = 0.0F;
    float pause_height = 0.0F;

    bool operator==(const HudSizing &) const = default;
};

struct HudRect {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

struct HudPlateSizes {
    HudRect energy;
    HudRect info;
    HudRect integrity;
};

struct HudPlacement {
    HudRect energy;
    HudRect info;
    HudRect integrity;
    HudRect tray;
    HudRect pause;
    bool scroll_cards = false;
};

struct HudCardArrangement {
    std::vector<HudRect> cards;
    float width = 0.0F;
    float height = 0.0F;
};

[[nodiscard]] HudCardArrangement arrange_hud_cards(std::span<const HudRect> cards, float available_width, float gap, bool wrap);

[[nodiscard]] HudSizing resolve_hud_sizing(const UiViewportMetrics &viewport, const HudCardLayout &cards = {});
[[nodiscard]] HudPlacement place_hud(const UiViewportMetrics &viewport, const HudSizing &sizing, const HudPlateSizes &plates, float cards_width,
                                     float cards_height);

} // namespace defn

#endif
