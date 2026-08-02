// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "score_screen_view.h"

#include "godot_string.h"
#include "owned_upgrades_panel.h"
#include "score_screen_view_model.h"
#include "ui_screen_scaffold.h"
#include "ui_sfx_player.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"
#include "upgrade_card_presenter.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/viewport.hpp>

#include <algorithm>

namespace defn {

namespace {

real_t get_score_screen_width(Node *parent) {
    const auto min_width = static_cast<real_t>(UiThemeProvider::data().metric("score_screen_min_width", 600));
    if (parent == nullptr || parent->get_viewport() == nullptr) {
        return min_width;
    }

    const real_t viewport_width = parent->get_viewport()->get_visible_rect().size.x;
    if (viewport_width <= 0.0) {
        return min_width;
    }

    const real_t horizontal_margins = static_cast<real_t>(UiThemeProvider::spacing("screen_margin")) * static_cast<real_t>(2);
    const real_t max_width = std::max<real_t>(static_cast<real_t>(0), viewport_width - horizontal_margins);
    const real_t target_width = viewport_width * UiThemeProvider::data().screen.content_max_width_ratio;
    return std::min(std::max(target_width, std::min(min_width, max_width)), max_width);
}

real_t selection_label_width() { return static_cast<real_t>(UiThemeProvider::data().metric("upgrade_selection_label_width", 240)); }

void add_stat_row(VBoxContainer *parent, const String &label_text, const String &value_text) { parent->add_child(make_stat_row(label_text, value_text)); }

void add_spacer(VBoxContainer *parent, const char *spacing_role) {
    parent->add_child(make_spacer(static_cast<real_t>(UiThemeProvider::spacing(spacing_role))));
}

void connect_if_valid(Button *button, const Callable &callable) {
    if (button != nullptr && callable.is_valid()) {
        button->connect("pressed", callable);
    }
}

void add_selection_label(VBoxContainer *label_column, const String &text, std::string_view text_style) {
    auto *label = make_label(text, text_style);
    label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    label->set_custom_minimum_size(godot::Vector2(selection_label_width(), 0));
    label_column->add_child(label);
}

VBoxContainer *add_label_column(HBoxContainer *row) {
    auto *label_column = memnew(VBoxContainer);
    label_column->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    label_column->set_custom_minimum_size(godot::Vector2(selection_label_width(), 0));
    label_column->add_theme_constant_override("separation", UiThemeProvider::spacing("xs"));
    row->add_child(label_column);
    return label_column;
}

void add_upgrade_selection(VBoxContainer *content, const ScoreScreenRewardModel &reward, const ScoreScreenViewModel &view_model,
                           const Callable &on_select_upgrade, UiSfxPlayer *ui_sfx_player) {
    if (content == nullptr || (reward.available_upgrades.empty() && view_model.new_unlocks.empty())) {
        return;
    }

    const std::string selected_upgrade_id = reward.selected_upgrade.has_value() ? reward.selected_upgrade->id : std::string();

    add_spacer(content, "md");

    auto *selection_row = memnew(HBoxContainer);
    selection_row->set_alignment(BoxContainer::ALIGNMENT_BEGIN);
    selection_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    selection_row->add_theme_constant_override("separation", UiThemeProvider::spacing("lg"));
    content->add_child(selection_row);

    auto *label_column = add_label_column(selection_row);

    if (view_model.reward_available) {
        add_selection_label(label_column, to_godot_string(view_model.reward_title), "reward_section");

        if (!view_model.reward_subtitle.empty()) {
            add_selection_label(label_column, to_godot_string(view_model.reward_subtitle), "body");
        }
    }

    for (const auto &new_unlock : view_model.new_unlocks) {
        add_selection_label(label_column, to_godot_string(new_unlock), "reward_section");
    }

    if (reward.available_upgrades.empty()) {
        return;
    }

    auto *card_row = memnew(HBoxContainer);
    card_row->set_alignment(BoxContainer::ALIGNMENT_BEGIN);
    card_row->add_theme_constant_override("separation", UiThemeProvider::spacing("md"));
    selection_row->add_child(card_row);

    for (const auto &card : reward.available_upgrades) {
        const bool selected = !selected_upgrade_id.empty() && card.id == selected_upgrade_id;

        Callable pressed_action;
        if (on_select_upgrade.is_valid() && !card.id.empty()) {
            pressed_action = on_select_upgrade.bind(to_godot_string(card.id));
        }

        auto *card_button = UpgradeCardPresenter::create(card, selected, false, Callable());
        connect_sfx(ui_sfx_player, card_button);
        connect_if_valid(card_button, pressed_action);
        card_row->add_child(card_button);
    }
}

void add_owned_upgrades_section(VBoxContainer *content, const std::vector<UpgradeCardViewModel> &owned_upgrades) {
    if (content == nullptr || owned_upgrades.empty()) {
        return;
    }

    add_spacer(content, "md");

    auto *owned_row = memnew(HBoxContainer);
    owned_row->set_alignment(BoxContainer::ALIGNMENT_BEGIN);
    owned_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    owned_row->add_theme_constant_override("separation", UiThemeProvider::spacing("lg"));
    content->add_child(owned_row);

    add_selection_label(add_label_column(owned_row), "YOUR UPGRADES", "owned_section");

    OwnedUpgradesPanel::Options owned_panel_options;
    owned_panel_options.min_size = godot::Vector2(0, static_cast<real_t>(UiThemeProvider::data().metric("owned_upgrades_strip_height", 240)));
    owned_panel_options.layout = OwnedUpgradesPanel::Layout::HorizontalStrip;

    auto *owned_panel = OwnedUpgradesPanel::build(owned_upgrades, owned_panel_options);
    owned_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    owned_row->add_child(owned_panel);
}

void add_action_button(HBoxContainer *row, const String &text, const Callable &action, bool enabled, UiSfxPlayer *ui_sfx_player) {
    auto *button = make_button(text, "secondary", action, ui_sfx_player);
    apply_enabled(button, enabled);
    row->add_child(button);
}

} // namespace

ScoreScreenViewNodes ScoreScreenView::show(Node *parent, const ScoreScreenModel &model, const ScoreScreenActions &actions, UiSfxPlayer *ui_sfx_player) {
    if (parent == nullptr) {
        return {};
    }

    const ScoreScreenViewModel presentation = ScoreScreenPresenter::build(model);

    const UiScreenScaffold scaffold = build_screen(parent, {.title = to_godot_string(presentation.title),
                                                            .title_text_style = "screen_display",
                                                            .show_backdrop = true,
                                                            .panelled_body = true,
                                                            .scrollable_body = false,
                                                            .constrain_height = false});
    if (scaffold.root == nullptr) {
        return {};
    }

    ScoreScreenViewNodes view;
    view.overlay = scaffold.root;
    view.panel = scaffold.panel;
    if (view.panel != nullptr) {
        godot::Vector2 panel_size = view.panel->get_custom_minimum_size();
        panel_size.x = get_score_screen_width(parent);
        view.panel->set_custom_minimum_size(panel_size);
    }

    if (scaffold.header->get_child_count() > 0) {
        set_state_tint(Object::cast_to<Control>(scaffold.header->get_child(0)), presentation.victory ? "victory" : "defeat");
    }

    VBoxContainer *content = scaffold.body;

    const size_t score_start_index = presentation.victory ? 5 : 4;
    for (size_t index = 0; index < std::min(score_start_index, presentation.stat_rows.size()); ++index) {
        add_stat_row(content, to_godot_string(presentation.stat_rows[index].first), to_godot_string(presentation.stat_rows[index].second));
    }

    auto *separator = memnew(ColorRect);
    separator->set_custom_minimum_size(godot::Vector2(0, static_cast<real_t>(UiThemeProvider::shape("border_width"))));
    separator->set_color(UiThemeProvider::color("border"));
    content->add_child(separator);

    for (size_t index = score_start_index; index < presentation.stat_rows.size(); ++index) {
        add_stat_row(content, to_godot_string(presentation.stat_rows[index].first), to_godot_string(presentation.stat_rows[index].second));
    }

    add_upgrade_selection(content, model.reward, presentation, actions.on_select_upgrade, ui_sfx_player);

    add_owned_upgrades_section(content, model.owned_upgrades);

    HBoxContainer *button_row = scaffold.footer;
    button_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);

    if (presentation.next_level_button_visible) {
        add_action_button(button_row, "Next Level", actions.on_next_level, presentation.next_level_button_enabled, ui_sfx_player);
    }

    add_action_button(button_row, "Retry", actions.on_retry, presentation.retry_button_enabled, ui_sfx_player);
    add_action_button(button_row, "Campaign", actions.on_campaign, presentation.campaign_button_enabled, ui_sfx_player);

    return view;
}

} // namespace defn
