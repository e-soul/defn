// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "upgrade_card_presenter.h"

#include "godot_string.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"

#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

constexpr real_t UPGRADE_CARD_TITLE_MIN_HEIGHT = 44.0;

} // namespace

Button *UpgradeCardPresenter::create(const UpgradeCardViewModel &upgrade_card, bool selected, bool disabled, const Callable &pressed_action) {
    auto *button = make_button({}, selected ? "card_selected" : "card", pressed_action);

    const auto content_margin = static_cast<real_t>(UiThemeProvider::spacing("lg"));
    const real_t text_width = button->get_custom_minimum_size().x - (content_margin * 2.0F);

    auto *content_margin_box = memnew(MarginContainer);
    content_margin_box->set_anchors_preset(Control::PRESET_FULL_RECT);
    content_margin_box->add_theme_constant_override("margin_left", static_cast<int>(content_margin));
    content_margin_box->add_theme_constant_override("margin_top", static_cast<int>(content_margin));
    content_margin_box->add_theme_constant_override("margin_right", static_cast<int>(content_margin));
    content_margin_box->add_theme_constant_override("margin_bottom", static_cast<int>(content_margin));
    content_margin_box->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    button->add_child(content_margin_box);

    auto *content = memnew(VBoxContainer);
    content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    content->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    content->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    content->add_theme_constant_override("separation", UiThemeProvider::spacing("sm"));
    content->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    content_margin_box->add_child(content);

    auto *emoji_label = make_label(upgrade_card.emoji.empty() ? String("?") : to_godot_string(upgrade_card.emoji), "card_emoji");
    emoji_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    content->add_child(emoji_label);

    if (upgrade_card.owned_count > 1) {
        auto *count_label = make_label(vformat("x%d", upgrade_card.owned_count), "card_count");
        count_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        content->add_child(count_label);
    }

    auto *name_label = make_label(upgrade_card.name.empty() ? String("Upgrade") : to_godot_string(upgrade_card.name), "body");
    name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    name_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
    name_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    name_label->set_custom_minimum_size(godot::Vector2(text_width, UPGRADE_CARD_TITLE_MIN_HEIGHT));
    content->add_child(name_label);

    auto *description_label = make_label(to_godot_string(upgrade_card.description), "card_body");
    description_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    description_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    description_label->set_custom_minimum_size(godot::Vector2(text_width, 0));
    content->add_child(description_label);

    button->set_disabled(disabled);
    return button;
}

} // namespace defn