// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "progression_stats_screen_view.h"
#include "ui_sfx_player.h"

#include "godot_string.h"
#include "owned_upgrades_panel.h"
#include "progression_stat_meter.h"
#include "progression_stats_presenter.h"
#include "ui_screen_scaffold.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/color.hpp>

namespace defn {

namespace {

godot::String frame_zero_path(const std::string &path_template) {
    godot::String path = to_godot_string(path_template);
    path = path.replace("%03d", "000");
    return path.replace("%d", "0");
}

godot::Ref<godot::Texture2D> load_portrait(const std::string &path_template) {
    if (path_template.empty()) {
        return {};
    }
    return godot::ResourceLoader::get_singleton()->load(frame_zero_path(path_template));
}

real_t metric(const char *name, int fallback) { return static_cast<real_t>(UiThemeProvider::data().metric(name, fallback)); }

} // namespace

void ProgressionStatsScreenView::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("select_entity", "entity_id"), &ProgressionStatsScreenView::select_entity);
    godot::ClassDB::bind_method(godot::D_METHOD("show_owned_upgrades"), &ProgressionStatsScreenView::show_owned_upgrades);
    godot::ClassDB::bind_method(godot::D_METHOD("show_dossier"), &ProgressionStatsScreenView::show_dossier);
    godot::ClassDB::bind_method(godot::D_METHOD("go_back"), &ProgressionStatsScreenView::go_back);
}

void ProgressionStatsScreenView::configure(ProgressionOverviewSnapshot snapshot, std::vector<UpgradeCardViewModel> owned_upgrades,
                                           const godot::Callable &back_action, UiSfxPlayer *ui_sfx_player) {
    snapshot_ = std::move(snapshot);
    owned_upgrades_ = std::move(owned_upgrades);
    back_action_ = back_action;
    ui_sfx_player_ = ui_sfx_player;
    selected_entity_id_ = ProgressionStatsPresenter::default_selection(snapshot_);
    showing_all_upgrades_ = false;
    set_name("ProgressionStatsScreen");
    set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
    set_v_size_flags(godot::Control::SIZE_EXPAND_FILL);
    rebuild();
}

void ProgressionStatsScreenView::select_entity(const godot::String &entity_id) {
    const std::string candidate = to_std_string(entity_id);
    for (const auto &entity : snapshot_.entities) {
        if (entity.id == candidate && entity.unlocked) {
            selected_entity_id_ = candidate;
            showing_all_upgrades_ = false;
            rebuild();
            return;
        }
    }
}

void ProgressionStatsScreenView::show_owned_upgrades() {
    showing_all_upgrades_ = true;
    rebuild();
}

void ProgressionStatsScreenView::show_dossier() {
    showing_all_upgrades_ = false;
    rebuild();
}

void ProgressionStatsScreenView::go_back() {
    if (back_action_.is_valid()) {
        back_action_.call();
    }
}

void ProgressionStatsScreenView::clear_content() {
    exact_detail_label_ = nullptr;
    active_stat_id_ = godot::String();
    while (get_child_count() > 0) {
        godot::Node *child = get_child(0);
        remove_child(child);
        child->queue_free();
    }
}

void ProgressionStatsScreenView::rebuild() {
    clear_content();
    const ProgressionStatsScreenViewModel model = ProgressionStatsPresenter::present(snapshot_, selected_entity_id_);
    selected_entity_id_ = model.selected_entity_id;

    const UiScreenScaffold scaffold = build_screen(this, {.title = showing_all_upgrades_ ? "ALL OWNED UPGRADES" : "COMMAND ROSTER",
                                                          .show_backdrop = false,
                                                          .panelled_body = true,
                                                          .scrollable_body = false,
                                                          .max_content_size = get_custom_minimum_size()});
    if (scaffold.root == nullptr) {
        return;
    }
    scaffold.footer->set_alignment(godot::BoxContainer::ALIGNMENT_CENTER);

    if (showing_all_upgrades_) {
        OwnedUpgradesPanel::Options options;
        options.min_size = {metric("progression_dossier_width", 880), metric("owned_upgrades_grid_height", 430)};
        options.layout = OwnedUpgradesPanel::Layout::VerticalGrid;
        options.grid_columns = 4;
        scaffold.body->add_child(OwnedUpgradesPanel::build(owned_upgrades_, options));
        auto *return_button =
            make_button("Return to Command Roster", "secondary", callable_mp(this, &ProgressionStatsScreenView::show_dossier), ui_sfx_player_);
        return_button->set_name("ReturnToDossierButton");
        scaffold.footer->add_child(return_button);
        auto *back = make_button(to_godot_string(model.back_label), "secondary", callable_mp(this, &ProgressionStatsScreenView::go_back), ui_sfx_player_);
        back->set_name("ProgressionBackButton");
        scaffold.footer->add_child(back);
        return;
    }

    auto *selector_scroll = memnew(godot::ScrollContainer);
    selector_scroll->set_name("EntitySelectorScroll");
    selector_scroll->set_horizontal_scroll_mode(godot::ScrollContainer::SCROLL_MODE_AUTO);
    selector_scroll->set_vertical_scroll_mode(godot::ScrollContainer::SCROLL_MODE_DISABLED);
    auto *selector_row = memnew(godot::HBoxContainer);
    selector_row->set_alignment(godot::BoxContainer::ALIGNMENT_CENTER);
    selector_row->add_theme_constant_override("separation", UiThemeProvider::spacing("sm"));
    selector_scroll->add_child(selector_row);
    for (const auto &selector : model.selectors) {
        godot::Callable pressed;
        if (selector.unlocked) {
            pressed = callable_mp(this, &ProgressionStatsScreenView::select_entity).bind(to_godot_string(selector.id));
        }
        auto *button = make_button(to_godot_string(selector.label), selector.selected ? "roster_selected" : "roster", pressed, ui_sfx_player_);
        button->set_name(to_godot_string("Selector_" + selector.id));
        button->set_focus_mode(godot::Control::FOCUS_ALL);
        apply_enabled(button, selector.unlocked);
        if (!selector.locked_message.empty()) {
            button->set_tooltip_text(to_godot_string(selector.locked_message));
        }
        if (const auto texture = load_portrait(selector.portrait_path_template); texture.is_valid()) {
            button->set_button_icon(texture);
            button->set_expand_icon(true);
        }
        selector_row->add_child(button);
    }
    scaffold.body->add_child(selector_scroll);

    auto *dossier = make_surface("dossier");
    dossier->set_name("EntityDossier");
    dossier->set_custom_minimum_size({metric("progression_dossier_width", 880), metric("progression_dossier_height", 330)});
    auto *columns = memnew(godot::HBoxContainer);
    columns->add_theme_constant_override("separation", UiThemeProvider::spacing("xl"));
    dossier->add_child(columns);

    auto *identity = memnew(godot::VBoxContainer);
    identity->set_custom_minimum_size({metric("dossier_identity_width", 390), 0.0F});
    const godot::Vector2 portrait_size{metric("dossier_portrait_width", 220), metric("dossier_portrait_height", 150)};
    if (const auto texture = load_portrait(model.portrait_path_template); texture.is_valid()) {
        auto *portrait = memnew(godot::TextureRect);
        portrait->set_name("EntityPortrait");
        portrait->set_custom_minimum_size(portrait_size);
        portrait->set_expand_mode(godot::TextureRect::EXPAND_IGNORE_SIZE);
        portrait->set_stretch_mode(godot::TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
        portrait->set_texture(texture);
        identity->add_child(portrait);
    } else {
        const std::string initial = model.title.empty() ? "?" : model.title.substr(0, 1);
        auto *fallback = make_label(to_godot_string(initial), "portrait_fallback");
        fallback->set_name("EntityPortraitFallback");
        fallback->set_custom_minimum_size(portrait_size);
        fallback->set_horizontal_alignment(godot::HORIZONTAL_ALIGNMENT_CENTER);
        fallback->set_vertical_alignment(godot::VERTICAL_ALIGNMENT_CENTER);
        identity->add_child(fallback);
    }
    auto *name = make_label(to_godot_string(model.title), "entity_name");
    name->set_horizontal_alignment(godot::HORIZONTAL_ALIGNMENT_CENTER);
    identity->add_child(name);
    auto *description = make_label(to_godot_string(model.description.empty() ? "No field briefing available." : model.description), "body");
    description->set_autowrap_mode(godot::TextServer::AUTOWRAP_WORD_SMART);
    description->set_horizontal_alignment(godot::HORIZONTAL_ALIGNMENT_CENTER);
    identity->add_child(description);
    columns->add_child(identity);

    auto *stats_column = memnew(godot::VBoxContainer);
    stats_column->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
    stats_column->add_theme_constant_override("separation", UiThemeProvider::spacing("sm"));
    for (const auto &stat : model.stats) {
        auto *row = memnew(godot::HBoxContainer);
        auto *label = make_label(to_godot_string(stat.label), "body");
        label->set_custom_minimum_size({metric("dossier_stat_label_width", 145), 0.0F});
        row->add_child(label);
        auto *meter = memnew(ProgressionStatMeter);
        meter->configure(stat.visual);
        meter->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
        meter->connect("detail_state_changed", callable_mp(this, &ProgressionStatsScreenView::on_stat_detail_changed));
        row->add_child(meter);
        stats_column->add_child(row);
    }
    exact_detail_label_ = make_label({}, "secondary");
    exact_detail_label_->set_name("ExactStatDetail");
    exact_detail_label_->set_custom_minimum_size({0.0F, metric("dossier_detail_height", 24)});
    exact_detail_label_->set_autowrap_mode(godot::TextServer::AUTOWRAP_WORD_SMART);
    stats_column->add_child(exact_detail_label_);
    stats_column->add_child(make_label("UPGRADE SOURCES", "subsection"));
    if (model.upgrades.empty()) {
        stats_column->add_child(make_label(to_godot_string(model.empty_upgrade_message), "muted"));
    } else {
        for (const auto &upgrade : model.upgrades) {
            stats_column->add_child(make_label(to_godot_string(upgrade.emoji + " " + upgrade.label), "body"));
        }
    }
    columns->add_child(stats_column);
    scaffold.body->add_child(dossier);

    auto *all_upgrades = make_button(to_godot_string(model.all_upgrades_label), "secondary",
                                     callable_mp(this, &ProgressionStatsScreenView::show_owned_upgrades), ui_sfx_player_);
    all_upgrades->set_name("AllOwnedUpgradesButton");
    scaffold.footer->add_child(all_upgrades);
    auto *back = make_button(to_godot_string(model.back_label), "secondary", callable_mp(this, &ProgressionStatsScreenView::go_back), ui_sfx_player_);
    back->set_name("ProgressionBackButton");
    scaffold.footer->add_child(back);
}

void ProgressionStatsScreenView::on_stat_detail_changed(const godot::String &stat_id, const godot::String &detail, bool active) {
    if (exact_detail_label_ == nullptr) {
        return;
    }
    if (active) {
        active_stat_id_ = stat_id;
        exact_detail_label_->set_text(detail);
    } else if (active_stat_id_ == stat_id) {
        active_stat_id_ = godot::String();
        exact_detail_label_->set_text(godot::String());
    }
}

} // namespace defn
