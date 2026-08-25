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
#include <algorithm>
#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace defn {

namespace {

/// The palette role each integrity band adopts. Both the shield tint and the meter fill read from here, so the
/// two never disagree about how badly the base is hurt.
std::string_view integrity_color_role(IntegrityTier tier) {
    switch (tier) {
    case IntegrityTier::INTACT:
        return "state_success";
    case IntegrityTier::DAMAGED:
        return "state_warning";
    case IntegrityTier::CRITICAL:
        return "integrity_critical";
    }
    return "state_success";
}

/// The reserved digit floor every plain numeric readout starts from.
int value_digit_floor() { return UiThemeProvider::data().metric("hud_min_value_digits", 3); }

} // namespace

void HudValueLabel::set_value(const String &text) {
    label->set_text(text);

    const int digits = std::max(floor_digits, static_cast<int>(text.length()));
    const Ref<Font> font = label->get_theme_font("font");
    if (digits <= reserved_digits || font.is_null()) {
        return;
    }
    reserved_digits = digits;

    // Measured from a repeated zero rather than the live text, so two values with the same digit count always
    // reserve the same width even in a font whose digits are not uniform.
    String sample;
    for (int index = 0; index < digits; ++index) {
        sample += "0";
    }

    const float needed = font->get_string_size(sample, HORIZONTAL_ALIGNMENT_LEFT, -1, label->get_theme_font_size("font_size")).x;
    const godot::Vector2 reserved = label->get_custom_minimum_size();
    label->set_custom_minimum_size({std::max(reserved.x, needed), reserved.y});
}

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
    UiSfxPlayer::install(this);
    build_ui();
}

void HUD::build_ui() {
    build_energy_plate();
    build_info_plate();
    build_integrity_plate();

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
    card_container->set_offset(Side::SIDE_BOTTOM, -UiThemeProvider::metric("hud_margin", 24));
    card_container->add_theme_constant_override("separation", UiThemeProvider::spacing("md"));
    add_child(card_container);

    refresh();
}

PanelContainer *HUD::build_plate(const char *name, std::string_view surface, Control::LayoutPreset preset) {
    auto *plate = make_surface(surface);
    plate->set_name(name);
    anchor_hud_pod(plate, preset);
    add_child(plate);
    return plate;
}

void HUD::build_energy_plate() {
    PanelContainer *plate = build_plate("EnergyPlate", "hud_pod", Control::PRESET_TOP_LEFT);

    const ReadoutRow group = make_readout("energy");
    group.row->add_child(make_readout_label("ENERGY", "hud_label"));
    energy_value_label = {.label = make_readout_label("0", "hud_value"), .floor_digits = value_digit_floor()};
    group.row->add_child(energy_value_label.label);
    plate->add_child(group.row);
}

void HUD::build_info_plate() {
    PanelContainer *plate = build_plate("InfoPlate", "hud_tag", Control::PRESET_CENTER_TOP);

    // Level, wave and score sit on one line; the wide gap between groups is what keeps them legible as
    // three separate readings rather than one run-on string.
    auto *row = memnew(HBoxContainer);
    row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    row->add_theme_constant_override("separation", UiThemeProvider::spacing("xl"));
    plate->add_child(row);

    // The level has no label: the name is the reading, and the flag already says what kind of reading it is.
    level_group = make_readout("level").row;
    level_group->set_name("LevelGroup");
    level_label = make_readout_label("", "hud_level");
    level_group->add_child(level_label);
    row->add_child(level_group);

    const ReadoutRow wave_group = make_readout("wave");
    wave_group.row->add_child(make_readout_label("WAVE", "hud_label"));
    // A wave counter has no floor worth reserving: it starts at one digit and only ever widens if a level runs long.
    wave_current_label = {.label = make_readout_label("1", "hud_wave")};
    wave_group.row->add_child(wave_current_label.label);
    wave_total_label = make_readout_label("/ 3", "hud_wave_total");
    wave_group.row->add_child(wave_total_label);
    row->add_child(wave_group.row);

    const ReadoutRow score_group = make_readout("score");
    score_group.row->add_child(make_readout_label("SCORE", "hud_label"));
    score_label = {.label = make_readout_label("0", "hud_score"), .floor_digits = value_digit_floor()};
    score_group.row->add_child(score_label.label);
    row->add_child(score_group.row);
}

void HUD::build_integrity_plate() {
    PanelContainer *plate = build_plate("IntegrityPlate", "hud_pod", Control::PRESET_TOP_RIGHT);

    const ReadoutRow group = make_readout("integrity");
    integrity_medallion = group.medallion;
    integrity_medallion.plate->set_name("IntegrityMedallion");
    group.row->add_child(make_readout_label("INTEGRITY", "hud_label"));

    integrity_meter = memnew(HudIntegrityMeter);
    integrity_meter->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
    group.row->add_child(integrity_meter);
    plate->add_child(group.row);
}

void HUD::set_friendly_units(const std::vector<UnitConfig> &units) {
    hud_input_.deploy_cards.clear();
    hud_input_.deploy_cards.reserve(units.size());
    for (const auto &cfg : units) {
        hud_input_.deploy_cards.push_back(build_deploy_card_presentation_input(cfg));
    }

    refresh();
}

void HUD::set_level(const String &level_name) {
    hud_input_.level_name = level_name.utf8().get_data();
    refresh();
}

void HUD::refresh() {
    // The readouts are built together in `build_ui`, so one check stands in for all of them and keeps every
    // render path free of per-node guards.
    if (card_container != nullptr) {
        render(HudPresenter::build(hud_input_));
    }
}

void HUD::render(const HudModel &model) {
    energy_value_label.set_value(to_godot_string(model.energy_text));
    wave_current_label.set_value(to_godot_string(model.wave.current_text));
    score_label.set_value(to_godot_string(model.score_text));
    wave_total_label->set_text(to_godot_string(model.wave.total_text));

    level_label->set_text(to_godot_string(model.level_text));
    level_group->set_visible(model.level_visible);

    render_integrity(model.integrity);
    render_deploy_cards(model.deploy_cards);
}

void HUD::render_integrity(const HudIntegrityModel &integrity) {
    const godot::Color color = UiThemeProvider::color(integrity_color_role(integrity.tier));

    // Re-tinting the shield rebuilds a style box and reloads its mark, so it only happens when the band actually
    // changes; the meter itself takes every reading and decides for itself whether it has to redraw.
    if (integrity_tier != integrity.tier) {
        integrity_tier = integrity.tier;
        apply_icon_medallion(integrity_medallion, theme_icon("integrity"), color);
    }
    integrity_meter->configure(integrity, color);
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
            button->connect("pressed", callable_mp(this, &HUD::on_card_pressed).bind(to_godot_string(card_model.card.unit_id)));
            card_container->add_child(button);
            deploy_cards.push_back({.unit_type = card_model.card.unit_id, .button = button});
        }
    }

    // Affordability is recomputed on every energy tick, but restyling a card is only worth it when it flips.
    for (size_t index = 0; index < cards.size(); ++index) {
        DeployCardUI &card = deploy_cards[index];
        if (card.button == nullptr || card.enabled == cards[index].enabled) {
            continue;
        }
        card.enabled = cards[index].enabled;
        apply_enabled(card.button, *card.enabled);
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

void HUD::on_card_pressed(const String &unit_type) { emit_signal("deploy_requested", unit_type); }

void HUD::update_core_resource(int value) {
    hud_input_.energy = value;
    refresh();
}

void HUD::update_wave(int current, int total) {
    hud_input_.current_wave = current;
    hud_input_.total_waves = total;
    refresh();
}

void HUD::update_integrity(int health, int max_health) {
    hud_input_.base_health = health;
    hud_input_.base_max_health = max_health;
    refresh();
}

void HUD::update_score(int score) {
    hud_input_.score = score;
    refresh();
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
                              });

    score_screen_overlay = view.overlay;
    score_screen_panel = view.panel;
}

void HUD::on_next_level_pressed(const String &level_id) { emit_signal("score_screen_next_level", level_id); }

void HUD::on_retry_pressed(const String &level_id) { emit_signal("score_screen_retry", level_id); }

void HUD::on_campaign_pressed() { emit_signal("score_screen_campaign"); }

void HUD::on_upgrade_card_pressed(const String &upgrade_id) { emit_signal("score_screen_upgrade_selected", upgrade_id); }

} // namespace defn
