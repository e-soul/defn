// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "hud_layout.h"

#include <algorithm>

namespace defn {

HudCardArrangement arrange_hud_cards(std::span<const HudRect> cards, float available_width, float gap, bool wrap) {
    HudCardArrangement arrangement;
    float row_width = 0.0F;
    float row_height = 0.0F;
    float row_top = 0.0F;
    size_t row_start = 0;
    const auto finish_row = [&]() {
        arrangement.width = std::max(arrangement.width, row_width);
        for (size_t index = row_start; index < arrangement.cards.size(); ++index) {
            auto &card = arrangement.cards[index];
            card.y = row_top + (row_height - card.height) * 0.5F;
        }
        row_top += row_height + gap;
        row_start = arrangement.cards.size();
        row_width = 0.0F;
        row_height = 0.0F;
    };
    for (const auto &card : cards) {
        if (wrap && row_width > 0.0F && row_width + gap + card.width > available_width) {
            finish_row();
        }
        const float card_left = row_width > 0.0F ? row_width + gap : 0.0F;
        arrangement.cards.push_back({.x = card_left, .y = row_top, .width = card.width, .height = card.height});
        row_width = card_left + card.width;
        row_height = std::max(row_height, card.height);
    }
    if (!cards.empty()) {
        finish_row();
        arrangement.height = row_top - gap;
    }
    return arrangement;
}

HudSizing resolve_hud_sizing(const UiViewportMetrics &viewport, const HudCardLayout &cards) {
    HudSizing sizing;
    if (viewport.width <= 0.0F || viewport.height <= 0.0F || viewport.css_width <= 0.0F || viewport.css_height <= 0.0F) {
        return sizing;
    }
    sizing.pixels_per_unit = std::min(viewport.css_width / viewport.width, viewport.css_height / viewport.height);
    sizing.responsive = viewport.web && (viewport.css_width < 1100.0F || (viewport.coarse_pointer && viewport.css_height < 700.0F));
    if (!sizing.responsive) {
        return sizing;
    }
    sizing.compact = viewport.css_width < 650.0F;
    sizing.wrap_cards = sizing.compact && viewport.css_height > viewport.css_width;
    const float units = 1.0F / sizing.pixels_per_unit;
    // Typography comes from the shared theme; short embedded canvases retain useful tap targets
    // rather than shrink with the world. Phone cards use a compact, single-row layout.
    sizing.readout_scale = std::max(1.0F, std::clamp(viewport.css_height * 0.085F, 28.0F, 40.0F) * units / 64.0F);
    sizing.margin = 8.0F * units;
    sizing.gap = 6.0F * units;
    sizing.level_width = std::clamp(viewport.css_width * 0.22F, 80.0F, 220.0F) * units;
    sizing.integrity_width = std::clamp(viewport.css_width * 0.12F, 42.0F, 100.0F) * units;
    sizing.card_height = std::max(44.0F, cards.height) * units;
    sizing.card_width = std::max(1.0F, cards.width) * units;
    sizing.card_inset = std::max(0.0F, cards.inset) * units;
    sizing.portrait_size = std::clamp(cards.portrait_size * units, 0.0F, std::max(0.0F, sizing.card_height - (2.0F * sizing.card_inset)));
    sizing.pause_width = 76.0F * units;
    sizing.pause_height = 44.0F * units;
    return sizing;
}

HudPlacement place_hud(const UiViewportMetrics &viewport, const HudSizing &sizing, const HudPlateSizes &plates, float cards_width, float cards_height) {
    const float margin = sizing.margin;
    const float gap = sizing.gap;
    const float full_width = std::max(1.0F, viewport.width - (2.0F * margin));
    const float overlay_width = viewport.overlay_width / sizing.pixels_per_unit;
    const float overlay_height = viewport.overlay_height / sizing.pixels_per_unit;
    const float top_width = std::max(1.0F, full_width - overlay_width - (overlay_width > 0.0F ? gap : 0.0F));
    HudPlacement placed{.energy = plates.energy, .info = plates.info, .integrity = plates.integrity};
    placed.energy.x = margin;
    placed.energy.y = margin;
    placed.integrity.x = margin + top_width - plates.integrity.width;
    placed.integrity.y = margin;
    placed.info.x = margin + (top_width - plates.info.width) * 0.5F;
    placed.info.y = margin;

    const float row_height = std::max(plates.energy.height, plates.integrity.height);
    placed.energy.height = row_height;
    placed.integrity.height = row_height;
    const float info_left = placed.energy.x + plates.energy.width + gap;
    const float info_right = placed.integrity.x - gap - plates.info.width;
    const bool center_fits = info_left <= info_right;
    if (center_fits) {
        placed.info.x = std::clamp(placed.info.x, info_left, info_right);
        const float height = std::max(row_height, plates.info.height);
        placed.energy.height = height;
        placed.info.height = height;
        placed.integrity.height = height;
    }
    if (!center_fits) {
        // The useful economy and integrity readings stay on the leading row. The operation/wave/score plate
        // gets the full width below both the row and any fullscreen browser button.
        if (plates.energy.width + gap + plates.integrity.width > top_width) {
            placed.integrity.x = viewport.width - margin - plates.integrity.width;
            placed.integrity.y = margin + std::max(row_height, overlay_height) + gap;
        }
        placed.info.x = margin + (full_width - plates.info.width) * 0.5F;
        placed.info.y = std::max({placed.energy.y + row_height, placed.integrity.y + row_height, overlay_height}) + gap;
    }

    placed.pause = {.x = viewport.width - margin - sizing.pause_width,
                    .y = viewport.height - margin - sizing.pause_height,
                    .width = sizing.pause_width,
                    .height = sizing.pause_height};
    const float tray_available = std::max(1.0F, full_width - (sizing.pause_width > 0.0F ? sizing.pause_width + gap : 0.0F));
    const float tray_width = std::min(cards_width, tray_available);
    placed.scroll_cards = cards_width > tray_available;
    // Centre on the battlefield whenever possible, moving left only enough to clear Pause.
    placed.tray = {.x = std::max(margin, std::min((viewport.width - tray_width) * 0.5F, margin + tray_available - tray_width)),
                   .y = viewport.height - margin - cards_height,
                   .width = tray_width,
                   .height = cards_height};
    return placed;
}

} // namespace defn
