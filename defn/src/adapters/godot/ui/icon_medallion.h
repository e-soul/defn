// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef ICON_MEDALLION_H
#define ICON_MEDALLION_H

#include "ui_theme_models.h"

#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/variant/color.hpp>

#include <string_view>

namespace defn {

/// A round plate carrying one tinted vector mark. The campaign map uses it for node states and the HUD for its
/// instrument icons, so both screens draw the same shape from the same theme data.
struct IconMedallionNodes {
    godot::Panel *plate = nullptr;
    godot::TextureRect *mark = nullptr;
};

/// Theme lookups that never borrow another entry's look: an unknown key yields the neutral model default.
[[nodiscard]] const UiMedallionStyle &theme_medallion(std::string_view key);
[[nodiscard]] const UiMedallionStyle &theme_hud_icon(std::string_view key);

/// Builds the plate and its mark. The pair carries no look until `apply_icon_medallion` tints it.
[[nodiscard]] IconMedallionNodes make_icon_medallion(float size);
void apply_icon_medallion(const IconMedallionNodes &nodes, const UiMedallionStyle &style, const godot::Color &tint);

} // namespace defn

#endif
