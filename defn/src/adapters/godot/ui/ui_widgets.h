// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include "icon_medallion.h"

#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/string.hpp>

#include <string_view>

namespace defn {

class UiSfxPlayer;

/// One instrument reading: a tinted medallion followed by whatever the caller adds after it. The medallion
/// comes back with the row because a reading such as base integrity re-tints its own as the value changes.
struct ReadoutRow {
    godot::HBoxContainer *row = nullptr;
    IconMedallionNodes medallion;
};

/// Which way a card stacks its icon against its text. Deploy cards and roster chips are wide and read across;
/// upgrade cards are tall and read down.
enum class CardLayout { Vertical, Horizontal };

struct CardSpec {
    /// The themed button variant the card wears. Its `min_size`, corner, border and padding all come from there.
    std::string_view variant = "card";
    CardLayout layout = CardLayout::Vertical;
    bool selected = false;
    /// A card with nothing to answer &mdash; an owned upgrade on display &mdash; should not behave as though it has.
    bool interactive = true;
};

/// The slots every card in the game is assembled from. A card fills whichever ones it has content for; the
/// frame, its padding and its selected treatment stay with the theme rather than with each call site.
struct CardNodes {
    godot::Button *button = nullptr;
    /// Holds the icon, then the text column, along the direction the layout asked for.
    godot::BoxContainer *body = nullptr;
    /// Title first, then whatever detail lines the card adds beneath it.
    godot::VBoxContainer *text = nullptr;
};

/// Shared control factories; every visual value comes from `UiThemeProvider`.
godot::Label *make_label(const godot::String &text, std::string_view text_style = "body");
godot::Button *make_button(const godot::String &text, std::string_view variant = "secondary", const godot::Callable &pressed = {}, UiSfxPlayer *sfx = nullptr);
/// Builds the shared card frame: a themed button, the padding its variant declares, and an icon-plus-text
/// body inside it. Deploy cards, upgrade cards and roster chips differ only in what they put in the slots.
CardNodes make_card(const CardSpec &spec, const godot::Callable &pressed = {}, UiSfxPlayer *sfx = nullptr);
/// Puts a card's icon ahead of its text column, whichever slot order the caller happened to fill first.
void add_card_icon(const CardNodes &card, godot::Control *icon);
/// The icon slot's usual filler: a texture scaled to fit a square without distorting it.
godot::TextureRect *make_card_portrait(const godot::Ref<godot::Texture2D> &texture, float size);
/// A theme mark on its own, tinted from its `icons` entry and sized for wherever it sits. The medallion is the
/// same mark on a plate; this is what an inline icon beside a line of text wants instead.
godot::TextureRect *make_icon(std::string_view icon_key, float size);
/// A card's name, in the one style the card family shares. A long name trims rather than spilling past the
/// frame, which the frame cannot prevent on its own: a Button is not a Container, so its children never grow it.
godot::Label *make_card_title(const godot::String &text);
godot::PanelContainer *make_surface(std::string_view surface_name = "panel");
godot::PanelContainer *make_chip(const godot::String &text, std::string_view color_role = "text_secondary");
godot::HBoxContainer *make_stat_row(const godot::String &label, const godot::String &value);
/// Every instrument icon in the game is this object: a medallion at `hud_icon_size`, tinted from its
/// `icons` entry. Sharing one construction is what keeps the HUD plates and the menu's career score
/// reading as the same instrument rather than two takes on one idea.
ReadoutRow make_readout(std::string_view icon_key);
/// Mixed type sizes have no shared baseline inside a BoxContainer, so each part of a readout is centred
/// against the row instead of resting wherever its own height leaves it.
godot::Label *make_readout_label(const godot::String &text, std::string_view text_style);
godot::VBoxContainer *make_stat_cell(const godot::String &heading, godot::Label *&value_out);
godot::HBoxContainer *make_action_bar();
godot::Control *make_spacer(float height);
/// Anchors an instrument plate so it grows from its own corner: the rect stays zero-width and Godot clamps it
/// up to the content's minimum size, which is what stops neighbours from shifting when a digit is added. The
/// floor on height keeps every plate level with the others however much each one carries.
void anchor_hud_pod(godot::Control *pod, godot::Control::LayoutPreset preset);

void apply_label_style(godot::Label *label, std::string_view text_style);
void apply_button_style(godot::Control *control, std::string_view variant);
void set_state_tint(godot::Control *control, std::string_view color_role);
void apply_enabled(godot::BaseButton *button, bool enabled);
/// Wires hover and press sound. The variant names which press sound, and an absent player falls back to the
/// active one, so no control can end up silent because its call site forgot.
void connect_sfx(UiSfxPlayer *sfx, godot::BaseButton *button, std::string_view variant = "secondary");

} // namespace defn

#endif
