// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "owned_upgrades_panel.h"

#include "ui_widgets.h"
#include "upgrade_card_presenter.h"

#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/callable.hpp>

#include <algorithm>

namespace defn {

Control *OwnedUpgradesPanel::build(const std::vector<UpgradeCardViewModel> &owned_upgrades, const Options &options) {
    if (owned_upgrades.empty()) {
        auto *empty_label = make_label(String::utf8("No upgrades yet — clear levels to earn them."), "empty_state");
        empty_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        empty_label->set_custom_minimum_size(options.min_size);
        return empty_label;
    }

    auto *scroll = memnew(ScrollContainer);
    scroll->set_custom_minimum_size(options.min_size);
    scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);

    if (options.layout == Layout::VerticalGrid) {
        scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
        scroll->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_AUTO);

        auto *card_grid = memnew(GridContainer);
        card_grid->set_columns(std::max(1, options.grid_columns));
        card_grid->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        card_grid->add_theme_constant_override("h_separation", options.card_separation);
        card_grid->add_theme_constant_override("v_separation", options.card_separation);
        scroll->add_child(card_grid);

        for (const auto &card : owned_upgrades) {
            card_grid->add_child(UpgradeCardPresenter::create(card, false, false, Callable(), false));
        }

        return scroll;
    }

    scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_AUTO);
    scroll->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);

    auto *card_row = memnew(HBoxContainer);
    card_row->add_theme_constant_override("separation", options.card_separation);
    scroll->add_child(card_row);

    for (const auto &card : owned_upgrades) {
        card_row->add_child(UpgradeCardPresenter::create(card, false, false, Callable(), false));
    }

    return scroll;
}

} // namespace defn
