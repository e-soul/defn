// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "hud.h"
#include "deploy_card_presenter.h"
#include "godot_color.h"
#include "godot_string.h"
#include "score_screen_view.h"
#include "ui_sfx_player.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"
#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <utility>

namespace defn {

HUD::HUD() = default;

void HUD::_bind_methods() {
    ADD_SIGNAL(MethodInfo("deploy_requested", PropertyInfo(Variant::STRING, "unit_type")));
    ADD_SIGNAL(MethodInfo("score_screen_next_level", PropertyInfo(Variant::STRING, "level_id")));
    ADD_SIGNAL(MethodInfo("score_screen_retry", PropertyInfo(Variant::STRING, "level_id")));
    ADD_SIGNAL(MethodInfo("score_screen_campaign"));
    ADD_SIGNAL(MethodInfo("score_screen_upgrade_selected", PropertyInfo(Variant::STRING, "upgrade_id")));
}

void HUD::_ready() {
    UiThemeProvider::install(get_tree());
    ui_sfx_player_ = memnew(UiSfxPlayer);
    ui_sfx_player_->set_name("UiSfxPlayer");
    add_child(ui_sfx_player_);
    ui_sfx_player_->configure(UiThemeProvider::data().sfx);
    build_ui();
}

void HUD::build_ui() {
    // ==========================================================
    // Top bar
    // ==========================================================
    auto *top_bar = memnew(HBoxContainer);
    top_bar->set_anchors_preset(Control::PRESET_TOP_WIDE);
    top_bar->set_offset(Side::SIDE_LEFT, 16.0);
    top_bar->set_offset(Side::SIDE_RIGHT, -16.0);
    top_bar->set_offset(Side::SIDE_TOP, 8.0);
    top_bar->set_offset(Side::SIDE_BOTTOM, 56.0);
    top_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    add_child(top_bar);

    // Energy label (left)
    core_resource_label = make_label(String::utf8("\u26A1 Energy: 100"), "hud_resource");
    core_resource_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    top_bar->add_child(core_resource_label);

    // Score label
    score_label = make_label("Score: 0", "hud_score");
    score_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    score_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    top_bar->add_child(score_label);

    // Wave label (center)
    wave_label = make_label("WAVE 1 / 3", "hud_wave");
    wave_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    wave_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    top_bar->add_child(wave_label);

    // Hearts container (right)
    hearts_container = memnew(HBoxContainer);
    hearts_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    hearts_container->set_alignment(BoxContainer::ALIGNMENT_END);
    top_bar->add_child(hearts_container);
    ensure_heart_icons(3);

    level_label = make_label("LEVEL", "hud_level");
    level_label->set_anchors_preset(Control::PRESET_TOP_WIDE);
    level_label->set_offset(Side::SIDE_LEFT, 16.0);
    level_label->set_offset(Side::SIDE_RIGHT, -16.0);
    level_label->set_offset(Side::SIDE_TOP, 52.0);
    level_label->set_offset(Side::SIDE_BOTTOM, 78.0);
    level_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    add_child(level_label);

    // ==========================================================
    // Deploy card container (bottom center)
    // ==========================================================
    card_container = memnew(HBoxContainer);
    card_container->set_anchor(SIDE_LEFT, 0.5);
    card_container->set_anchor(SIDE_RIGHT, 0.5);
    card_container->set_anchor(SIDE_TOP, 1.0);
    card_container->set_anchor(SIDE_BOTTOM, 1.0);
    card_container->set_h_grow_direction(Control::GROW_DIRECTION_BOTH);
    card_container->set_v_grow_direction(Control::GROW_DIRECTION_BEGIN);
    card_container->set_offset(Side::SIDE_BOTTOM, -10.0);
    card_container->add_theme_constant_override("separation", UiThemeProvider::spacing("md"));
    add_child(card_container);

    render(HudPresenter::build(hud_input_));
}

void HUD::set_friendly_units(const std::vector<UnitConfig> &units) {
    hud_input_.deploy_cards.clear();
    hud_input_.deploy_cards.reserve(units.size());
    for (const auto &cfg : units) {
        hud_input_.deploy_cards.push_back(build_deploy_card_presentation_input(cfg));
    }

    render(HudPresenter::build(hud_input_));
}

void HUD::set_level(int level_number, const String &level_name) {
    hud_input_.level_number = level_number;
    hud_input_.level_name = level_name.utf8().get_data();
    render(HudPresenter::build(hud_input_));
}

void HUD::render(const HudModel &model) {
    if (core_resource_label != nullptr) {
        core_resource_label->set_text(to_godot_string(model.energy_text));
    }
    if (score_label != nullptr) {
        score_label->set_text(to_godot_string(model.score_text));
    }
    if (wave_label != nullptr) {
        wave_label->set_text(to_godot_string(model.wave_text));
    }
    if (level_label != nullptr) {
        level_label->set_text(to_godot_string(model.level_text));
        level_label->set_visible(model.level_visible);
    }

    ensure_heart_icons(model.visible_hearts);
    for (int i = 0; std::cmp_less(i, heart_icons.size()); ++i) {
        heart_icons[i]->set_visible(i < model.visible_hearts);
    }

    render_deploy_cards(model.deploy_cards);
}

void HUD::render_deploy_cards(const std::vector<HudDeployCardModel> &cards) {
    if (card_container == nullptr) {
        return;
    }

    bool needs_rebuild = deploy_cards.size() != cards.size();
    for (size_t index = 0; !needs_rebuild && index < cards.size(); ++index) {
        needs_rebuild = deploy_cards[index].unit_type != cards[index].card.unit_id;
    }

    if (needs_rebuild) {
        clear_deploy_cards();
        deploy_cards.reserve(cards.size());
        for (const auto &card_model : cards) {
            auto *button = DeployCardPresenter::create(card_model.card, Callable());
            if (ui_sfx_player_ != nullptr) {
                ui_sfx_player_->connect_deploy_card(button);
            }
            button->connect("pressed", callable_mp(this, &HUD::on_card_pressed).bind(to_godot_string(card_model.card.unit_id)));
            card_container->add_child(button);
            deploy_cards.push_back({.unit_type = card_model.card.unit_id, .button = button});
        }
    }

    for (size_t index = 0; index < cards.size(); ++index) {
        Button *button = deploy_cards[index].button;
        if (button == nullptr) {
            continue;
        }
        const bool enabled = cards[index].enabled;
        apply_enabled(button, enabled);
    }
}

void HUD::clear_deploy_cards() {
    if (card_container != nullptr) {
        while (card_container->get_child_count() > 0) {
            Node *child = card_container->get_child(0);
            card_container->remove_child(child);
            child->queue_free();
        }
    }
    deploy_cards.clear();
}

void HUD::ensure_heart_icons(int count) {
    if (hearts_container == nullptr) {
        return;
    }

    while (std::cmp_less(heart_icons.size(), count)) {
        auto *heart = make_label(String::utf8("\u2665"), "hud_heart");
        hearts_container->add_child(heart);
        heart_icons.push_back(heart);
    }
}

void HUD::on_card_pressed(const String &unit_type) { emit_signal("deploy_requested", unit_type); }

void HUD::update_core_resource(int value) {
    hud_input_.energy = value;
    render(HudPresenter::build(hud_input_));
}

void HUD::update_wave(int current, int total) {
    hud_input_.current_wave = current;
    hud_input_.total_waves = total;
    render(HudPresenter::build(hud_input_));
}

void HUD::update_hearts(int integrity) {
    hud_input_.hearts = integrity;
    render(HudPresenter::build(hud_input_));
}

void HUD::update_card_affordability(int energy) {
    hud_input_.energy = energy;
    render(HudPresenter::build(hud_input_));
}

void HUD::update_score(int score) {
    hud_input_.score = score;
    render(HudPresenter::build(hud_input_));
}

void HUD::show_match_result_banner(const MatchResultCutsceneModel &model) {
    hide_match_result_banner();

    match_result_overlay = memnew(ColorRect);
    match_result_overlay->set_name("MatchResultBannerOverlay");
    match_result_overlay->set_anchors_preset(Control::PRESET_FULL_RECT);
    match_result_overlay->set_offset(SIDE_LEFT, 0.0);
    match_result_overlay->set_offset(SIDE_RIGHT, 0.0);
    match_result_overlay->set_offset(SIDE_TOP, 0.0);
    match_result_overlay->set_offset(SIDE_BOTTOM, 0.0);
    match_result_overlay->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    match_result_overlay->set_color(UiThemeProvider::color(model.victory ? "overlay_victory" : "overlay_defeat"));
    add_child(match_result_overlay);

    match_result_label = make_label(to_godot_string(model.label), "banner");
    match_result_label->set_name("MatchResultBannerLabel");
    match_result_label->set_anchors_preset(Control::PRESET_FULL_RECT);
    match_result_label->set_offset(SIDE_LEFT, 0.0);
    match_result_label->set_offset(SIDE_RIGHT, 0.0);
    match_result_label->set_offset(SIDE_TOP, 0.0);
    match_result_label->set_offset(SIDE_BOTTOM, 0.0);
    match_result_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    match_result_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
    match_result_label->add_theme_color_override("font_color", to_godot_color(model.label_color));
    match_result_label->add_theme_color_override("font_outline_color", to_godot_color(model.label_outline_color));
    match_result_overlay->add_child(match_result_label);
}

void HUD::hide_match_result_banner() {
    if (match_result_overlay != nullptr && !match_result_overlay->is_queued_for_deletion()) {
        if (match_result_overlay->get_parent() == this) {
            remove_child(match_result_overlay);
        }
        match_result_overlay->queue_free();
    }
    match_result_overlay = nullptr;
    match_result_label = nullptr;
}

void HUD::show_score_screen(const ScoreScreenModel &summary) {
    hide_match_result_banner();

    if (score_screen_overlay != nullptr && !score_screen_overlay->is_queued_for_deletion()) {
        score_screen_overlay->queue_free();
    }

    const ScoreScreenViewNodes view =
        ScoreScreenView::show(this, summary,
                              {
                                  .on_next_level = callable_mp(this, &HUD::on_next_level_pressed).bind(to_godot_string(summary.next_level_id)),
                                  .on_retry = callable_mp(this, &HUD::on_retry_pressed).bind(to_godot_string(summary.current_level_id)),
                                  .on_campaign = callable_mp(this, &HUD::on_campaign_pressed),
                                  .on_select_upgrade = callable_mp(this, &HUD::on_upgrade_card_pressed),
                              },
                              ui_sfx_player_);

    score_screen_overlay = view.overlay;
    score_screen_panel = view.panel;
}

void HUD::on_next_level_pressed(const String &level_id) { emit_signal("score_screen_next_level", level_id); }

void HUD::on_retry_pressed(const String &level_id) { emit_signal("score_screen_retry", level_id); }

void HUD::on_campaign_pressed() { emit_signal("score_screen_campaign"); }

void HUD::on_upgrade_card_pressed(const String &upgrade_id) { emit_signal("score_screen_upgrade_selected", upgrade_id); }

} // namespace defn
