// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "upgrade_card_presenter.h"

#include "godot_string.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"

#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

Button *UpgradeCardPresenter::create(const UpgradeCardViewModel &upgrade_card, bool selected, bool disabled, const Callable &pressed_action, bool interactive) {
    const CardNodes card = make_card({.variant = "card", .layout = CardLayout::Vertical, .selected = selected, .interactive = interactive}, pressed_action);

    add_card_icon(card, make_icon(upgrade_card.icon.empty() ? "generic" : upgrade_card.icon, UiThemeProvider::metric("upgrade_icon_size", 44)));

    if (upgrade_card.owned_count > 1) {
        auto *count_label = make_label(vformat("x%d", upgrade_card.owned_count), "card_count");
        count_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        card.text->add_child(count_label);
    }

    // The card is a fixed width, so both blocks wrap inside the padding the variant declared rather than
    // against a width each presenter works out for itself.
    auto *name_label = make_label(upgrade_card.name.empty() ? String("Upgrade") : to_godot_string(upgrade_card.name), "body");
    name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    name_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
    name_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    name_label->set_custom_minimum_size({0.0F, UiThemeProvider::metric("card_title_height", 44)});
    card.text->add_child(name_label);

    auto *description_label = make_label(to_godot_string(upgrade_card.description), "card_body");
    description_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    description_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    card.text->add_child(description_label);

    card.button->set_disabled(disabled);
    return card.button;
}

} // namespace defn
