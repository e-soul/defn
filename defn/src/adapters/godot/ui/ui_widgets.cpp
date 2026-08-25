// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "ui_widgets.h"

#include "ui_sfx_player.h"
#include "ui_theme_provider.h"

#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/core/memory.hpp>

#include <string>

namespace defn {

using namespace godot;

namespace {

constexpr float DISABLED_MODULATE_ALPHA = 0.7F;
constexpr float DISABLED_MODULATE_VALUE = 0.5F;

/// How far a plate sits from the edge it is anchored to, and which way it grows from there. Deriving both from
/// the preset is what keeps a right-anchored plate from ever being told to grow rightwards off the screen.
struct PodAnchor {
    real_t left;
    Control::GrowDirection horizontal;
};

PodAnchor pod_anchor(Control::LayoutPreset preset) {
    const real_t margin = UiThemeProvider::metric("hud_margin", 24);
    switch (preset) {
    case Control::PRESET_TOP_LEFT:
        return {.left = margin, .horizontal = Control::GROW_DIRECTION_END};
    case Control::PRESET_TOP_RIGHT:
        return {.left = -margin, .horizontal = Control::GROW_DIRECTION_BEGIN};
    default:
        return {.left = 0.0F, .horizontal = Control::GROW_DIRECTION_BOTH};
    }
}

} // namespace

void anchor_hud_pod(Control *pod, Control::LayoutPreset preset) {
    if (pod == nullptr) {
        return;
    }

    const PodAnchor anchor = pod_anchor(preset);
    const real_t top = UiThemeProvider::metric("hud_margin", 24);

    pod->set_custom_minimum_size({0.0F, UiThemeProvider::metric("hud_plate_height", 64)});
    pod->set_anchors_preset(preset);
    pod->set_h_grow_direction(anchor.horizontal);
    pod->set_v_grow_direction(Control::GROW_DIRECTION_END);
    pod->set_offset(Side::SIDE_LEFT, anchor.left);
    pod->set_offset(Side::SIDE_RIGHT, anchor.left);
    pod->set_offset(Side::SIDE_TOP, top);
    pod->set_offset(Side::SIDE_BOTTOM, top);
    pod->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
}

void apply_label_style(Label *label, std::string_view text_style) {
    if (label != nullptr) {
        UiThemeProvider::apply_to(label);
        label->set_theme_type_variation(UiThemeProvider::label_variation(text_style));
    }
}

void apply_button_style(Control *control, std::string_view variant) {
    if (control != nullptr) {
        UiThemeProvider::apply_to(control);
        control->set_theme_type_variation(UiThemeProvider::button_variation(variant));
    }
}

Label *make_label(const String &text, std::string_view text_style) {
    auto *label = memnew(Label);
    label->set_text(text);
    label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    apply_label_style(label, text_style);
    return label;
}

Button *make_button(const String &text, std::string_view variant, const Callable &pressed) {
    auto *button = memnew(Button);
    button->set_text(text);
    // Every control takes focus, so the game is navigable by keyboard and pad. The theme gives focus its own
    // border colour, distinct from both an ordinary border and the accent a selected card wears.
    button->set_focus_mode(Control::FOCUS_ALL);
    apply_button_style(button, variant);

    const UiButtonVariant *button_style = UiThemeProvider::data().find_button(variant);
    if (button_style != nullptr && (button_style->min_width > 0 || button_style->min_height > 0)) {
        button->set_custom_minimum_size({static_cast<real_t>(button_style->min_width), static_cast<real_t>(button_style->min_height)});
    }

    connect_sfx(button, variant);
    if (pressed.is_valid()) {
        button->connect("pressed", pressed);
    }
    return button;
}

CardNodes make_card(const CardSpec &spec, const Callable &pressed) {
    CardNodes card;

    const std::string variation = spec.selected ? std::string(spec.variant) + "_selected" : std::string(spec.variant);
    card.button = make_button({}, variation, pressed);
    card.button->set_clip_text(true);
    if (!spec.interactive) {
        // A card that answers nothing should not hover, click or take focus as though it might.
        card.button->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        card.button->set_focus_mode(Control::FOCUS_NONE);
    }

    // A Button is not a Container, so the content margin on its stylebox only positions the text Godot draws
    // itself. Reading the same role here is what gives every card the padding its variant actually declared,
    // instead of each presenter inventing an inset of its own.
    const UiButtonVariant *variant = UiThemeProvider::data().find_button(variation);
    const int inset = variant == nullptr || variant->content_margin_role.empty() ? 0 : UiThemeProvider::spacing(variant->content_margin_role);

    auto *margins = memnew(MarginContainer);
    margins->set_name("CardMargins");
    margins->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    margins->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    for (const char *side : {"margin_left", "margin_top", "margin_right", "margin_bottom"}) {
        margins->add_theme_constant_override(side, inset);
    }
    card.button->add_child(margins);

    card.body = spec.layout == CardLayout::Horizontal ? static_cast<BoxContainer *>(memnew(HBoxContainer)) : static_cast<BoxContainer *>(memnew(VBoxContainer));
    card.body->set_name("CardBody");
    card.body->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    card.body->add_theme_constant_override("separation", UiThemeProvider::spacing("sm"));
    card.body->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    margins->add_child(card.body);

    card.text = memnew(VBoxContainer);
    card.text->set_name("CardText");
    card.text->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    card.text->set_v_size_flags(spec.layout == CardLayout::Horizontal ? Control::SIZE_SHRINK_CENTER : Control::SIZE_EXPAND_FILL);
    card.text->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    card.text->add_theme_constant_override("separation", UiThemeProvider::spacing("xs"));
    card.text->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    card.body->add_child(card.text);

    return card;
}

void add_card_icon(const CardNodes &card, Control *icon) {
    if (card.body == nullptr || icon == nullptr) {
        return;
    }
    icon->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    // The icon holds its own size and centres on the axis the text runs along, so it sits on the card's
    // centre line whichever way the card is laid out.
    if (card.body->is_vertical()) {
        icon->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
    } else {
        icon->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
    }
    card.body->add_child(icon);
    card.body->move_child(icon, 0);
}

TextureRect *make_card_portrait(const Ref<Texture2D> &texture, float size) {
    auto *portrait = memnew(TextureRect);
    portrait->set_name("CardPortrait");
    portrait->set_custom_minimum_size({size, size});
    portrait->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
    portrait->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
    portrait->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
    portrait->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    portrait->set_texture(texture);
    return portrait;
}

TextureRect *make_icon(std::string_view icon_key, float size) {
    const UiMedallionStyle &style = theme_icon(icon_key);
    auto *mark = memnew(TextureRect);
    mark->set_name("Icon");
    mark->set_custom_minimum_size({size, size});
    mark->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
    mark->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
    mark->set_texture_filter(CanvasItem::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS);
    mark->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    mark->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
    mark->set_texture(theme_mark_texture(style));
    mark->set_modulate(UiThemeProvider::color(style.color_role));
    return mark;
}

Label *make_card_title(const String &text) {
    Label *title = make_label(text, "card_title");
    title->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    title->set_clip_text(true);
    title->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
    return title;
}

PanelContainer *make_surface(std::string_view surface_name) {
    auto *panel = memnew(PanelContainer);
    UiThemeProvider::apply_to(panel);
    panel->set_theme_type_variation(UiThemeProvider::panel_variation(surface_name));
    return panel;
}

PanelContainer *make_chip(const String &text, std::string_view color_role) {
    auto *chip = make_surface("chip");
    auto *label = make_label(text, "secondary");
    label->add_theme_color_override("font_color", UiThemeProvider::color(color_role));
    chip->add_child(label);
    return chip;
}

HBoxContainer *make_stat_row(const String &label, const String &value) {
    auto *row = memnew(HBoxContainer);
    row->set_h_size_flags(Control::SIZE_EXPAND_FILL);

    auto *label_control = make_label(label, "score_stat");
    label_control->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    row->add_child(label_control);

    auto *value_control = make_label(value, "score_value");
    value_control->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    row->add_child(value_control);

    return row;
}

ReadoutRow make_readout(std::string_view icon_key) {
    ReadoutRow readout;

    readout.row = memnew(HBoxContainer);
    readout.row->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
    // Parts sit close together so the group reads as a unit against the wider gaps separating it from its
    // neighbours on the same plate.
    readout.row->add_theme_constant_override("separation", UiThemeProvider::spacing("sm"));

    readout.medallion = make_icon_medallion(UiThemeProvider::metric("hud_icon_size", 38));
    readout.medallion.plate->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
    const UiMedallionStyle &style = theme_icon(icon_key);
    apply_icon_medallion(readout.medallion, style, UiThemeProvider::color(style.color_role));
    readout.row->add_child(readout.medallion.plate);

    return readout;
}

Label *make_readout_label(const String &text, std::string_view text_style) {
    Label *label = make_label(text, text_style);
    label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
    return label;
}

VBoxContainer *make_stat_cell(const String &heading, Label *&value_out) {
    auto *cell = memnew(VBoxContainer);
    cell->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    cell->add_theme_constant_override("separation", UiThemeProvider::spacing("xs"));

    auto *heading_label = make_label(heading, "stat_name");
    heading_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    cell->add_child(heading_label);

    value_out = make_label({}, "stat_value");
    value_out->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    cell->add_child(value_out);

    return cell;
}

HBoxContainer *make_action_bar() {
    auto *bar = memnew(HBoxContainer);
    bar->set_alignment(BoxContainer::ALIGNMENT_END);
    bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    bar->add_theme_constant_override("separation", UiThemeProvider::spacing(UiThemeProvider::data().screen.footer_gap_role));
    return bar;
}

Control *make_spacer(float height) {
    auto *spacer = memnew(Control);
    spacer->set_custom_minimum_size({0.0F, height});
    spacer->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    return spacer;
}

void set_state_tint(Control *control, std::string_view color_role) {
    if (control != nullptr) {
        control->add_theme_color_override("font_color", UiThemeProvider::color(color_role));
    }
}

void apply_enabled(BaseButton *button, bool enabled) {
    if (button != nullptr) {
        button->set_disabled(!enabled);
        button->set_modulate(enabled ? godot::Color(1, 1, 1, 1)
                                     : godot::Color(DISABLED_MODULATE_VALUE, DISABLED_MODULATE_VALUE, DISABLED_MODULATE_VALUE, DISABLED_MODULATE_ALPHA));
    }
}

void connect_sfx(BaseButton *button, std::string_view variant) {
    // Whatever builds a control wires it, and the player it wires to is the one the screen installed, so a
    // control is never handed a player a second caller thinks is the right one. The variant picks the sound.
    UiSfxPlayer *player = UiSfxPlayer::active();
    if (player == nullptr) {
        return;
    }
    const UiButtonVariant *style = UiThemeProvider::data().find_button(variant);
    player->connect_button(button, style == nullptr ? "click" : style->sfx_role);
}

} // namespace defn
