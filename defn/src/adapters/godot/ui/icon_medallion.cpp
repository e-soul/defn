// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "icon_medallion.h"

#include "godot_string.h"
#include "ui_theme_provider.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/core/memory.hpp>

namespace defn {

using namespace godot;

namespace {

constexpr std::string_view PLATE_SURFACE = "medallion";

const UiMedallionStyle &or_neutral(const UiMedallionStyle *found) {
    static const UiMedallionStyle unstyled;
    return found != nullptr ? *found : unstyled;
}

} // namespace

const UiMedallionStyle &theme_medallion(std::string_view key) { return or_neutral(UiThemeProvider::data().find_medallion(key)); }

const UiMedallionStyle &theme_icon(std::string_view key) { return or_neutral(UiThemeProvider::data().find_icon(key)); }

Ref<Texture2D> theme_mark_texture(const UiMedallionStyle &style) {
    if (style.mark.empty()) {
        return {};
    }
    return ResourceLoader::get_singleton()->load(to_godot_string(style.mark));
}

IconMedallionNodes make_icon_medallion(float size) {
    IconMedallionNodes nodes;

    nodes.plate = memnew(Panel);
    nodes.plate->set_name("Medallion");
    nodes.plate->set_custom_minimum_size({size, size});
    nodes.plate->set_size({size, size});
    nodes.plate->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);

    // The mark fills the whole plate; its inset is baked into the SVG viewBox, so it stays centred at any scale.
    nodes.mark = memnew(TextureRect);
    nodes.mark->set_name("Mark");
    nodes.mark->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    nodes.mark->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
    nodes.mark->set_stretch_mode(TextureRect::STRETCH_SCALE);
    nodes.mark->set_texture_filter(CanvasItem::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS);
    nodes.mark->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    nodes.plate->add_child(nodes.mark);

    return nodes;
}

void apply_icon_medallion(const IconMedallionNodes &nodes, const UiMedallionStyle &style, const godot::Color &tint) {
    if (nodes.plate == nullptr || nodes.mark == nullptr) {
        return;
    }

    Ref<StyleBoxFlat> plate_style = UiThemeProvider::surface(PLATE_SURFACE);
    plate_style->set_border_color(tint);
    nodes.plate->add_theme_stylebox_override("panel", plate_style);

    nodes.mark->set_texture(theme_mark_texture(style));
    nodes.mark->set_modulate(tint);
}

} // namespace defn
