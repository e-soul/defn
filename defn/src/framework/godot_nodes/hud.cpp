// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "hud.h"
#include "deploy_card_presenter.h"
#include "godot_color.h"
#include "godot_string.h"
#include "score_screen_view.h"
#include "ui_sfx_player.h"
#include "ui_theme_provider.h"
#include "ui_viewport_metrics.h"
#include "ui_widgets.h"
#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/h_scroll_bar.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/viewport.hpp>
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

int readout_font_size(Label *label) {
    return label->has_theme_font_size_override("font_size") ? label->get_theme_font_size("font_size") : UiThemeProvider::font_size("body");
}

void set_rect(Control *control, const HudRect &rect) {
    if (control->get_anchor(SIDE_LEFT) != 0.0F || control->get_anchor(SIDE_TOP) != 0.0F || control->get_anchor(SIDE_RIGHT) != 0.0F ||
        control->get_anchor(SIDE_BOTTOM) != 0.0F) {
        control->set_anchors_preset(Control::PRESET_TOP_LEFT);
    }
    const godot::Vector2 position{rect.x, rect.y};
    const godot::Vector2 size{rect.width, rect.height};
    if (control->get_position() != position) {
        control->set_position(position);
    }
    if (control->get_size() != size) {
        control->set_size(size);
    }
}

HudRect minimum_rect(Control *control) {
    const godot::Vector2 size = control->get_combined_minimum_size();
    return {.width = size.x, .height = size.y};
}

Ref<StyleBoxFlat> scaled_surface(std::string_view surface, float scale) {
    Ref<StyleBoxFlat> style = UiThemeProvider::surface(surface);
    for (const Side side : {SIDE_LEFT, SIDE_TOP, SIDE_RIGHT, SIDE_BOTTOM}) {
        style->set_content_margin(side, style->get_content_margin(side) * scale);
    }
    return style;
}

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

    const float needed = font->get_string_size(sample, HORIZONTAL_ALIGNMENT_LEFT, -1, readout_font_size(label)).x;
    const godot::Vector2 reserved = label->get_custom_minimum_size();
    label->set_custom_minimum_size({std::max(reserved.x, needed), reserved.y});
}

void HudValueLabel::remeasure() {
    const int digits = std::max(floor_digits, reserved_digits);
    const Ref<Font> font = label->get_theme_font("font");
    if (font.is_null()) {
        return;
    }
    String sample;
    for (int index = 0; index < digits; ++index) {
        sample += "0";
    }
    reserved_digits = digits;
    label->set_custom_minimum_size({font->get_string_size(sample, HORIZONTAL_ALIGNMENT_LEFT, -1, readout_font_size(label)).x, 0.0F});
}

HUD::HUD() = default;

void HUD::_bind_methods() {
    ADD_SIGNAL(MethodInfo("deploy_requested", PropertyInfo(Variant::STRING, "unit_type")));
    ADD_SIGNAL(MethodInfo("score_screen_next_level", PropertyInfo(Variant::STRING, "level_id")));
    ADD_SIGNAL(MethodInfo("score_screen_retry", PropertyInfo(Variant::STRING, "level_id")));
    ADD_SIGNAL(MethodInfo("score_screen_endless"));
    ADD_SIGNAL(MethodInfo("score_screen_campaign"));
    ADD_SIGNAL(MethodInfo("score_screen_upgrade_selected", PropertyInfo(Variant::STRING, "upgrade_id")));
}

void HUD::_ready() {
    UiThemeProvider::install(get_tree());
    UiSfxPlayer::install(this);
    build_ui();
    viewport_metrics = measure_ui_viewport(get_viewport());
    sizing = DeployCardPresenter::resolve_sizing(viewport_metrics);
    apply_readout_sizing();
    layout_ui();
}

void HUD::_process(double delta) {
    viewport_poll_seconds += delta;
    if (viewport_poll_seconds >= 0.2) {
        viewport_poll_seconds = 0.0;
        const UiViewportMetrics measured = measure_ui_viewport(get_viewport());
        UiThemeProvider::update_typography(measured);
        if (measured != viewport_metrics) {
            viewport_metrics = measured;
            sizing = DeployCardPresenter::resolve_sizing(viewport_metrics);
            apply_readout_sizing();
            for (const DeployCardUI &card : deploy_cards) {
                DeployCardPresenter::apply_sizing(card.button, sizing);
            }
            layout_dirty = true;
        }
    }
    if (layout_dirty) {
        layout_ui();
    }
}

void HUD::build_ui() {
    build_energy_plate();
    build_info_plate();
    build_integrity_plate();
    collect_readout_sizes(energy_plate);
    collect_readout_sizes(info_plate);
    collect_readout_sizes(integrity_plate);

    // ==========================================================
    // Deploy card container (bottom center)
    // ==========================================================
    card_tray = memnew(ScrollContainer);
    card_tray->set_name("DeployTray");
    card_tray->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_AUTO);
    card_tray->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
    // Reveal a card when focus actually enters it, rather than re-following the same card on every sort of
    // the container. Energy ticks must not undo the player's scroll position.
    card_tray->set_follow_focus(false);
    card_tray->connect("scroll_started", callable_mp(this, &HUD::on_deploy_scroll_started));
    card_tray->set_mouse_filter(Control::MOUSE_FILTER_STOP);
    add_child(card_tray);
    card_container = memnew(Control);
    card_container->set_name("DeployCards");
    card_container->set_mouse_filter(Control::MOUSE_FILTER_PASS);
    card_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    card_tray->add_child(card_container);
    card_scroll_hint = make_label("SWIPE FOR MORE  >", "hud_label");
    card_scroll_hint->set_name("DeployScrollHint");
    card_scroll_hint->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    card_scroll_hint->hide();
    add_child(card_scroll_hint);

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
    energy_plate = plate;

    const ReadoutRow group = make_readout("energy");
    group.row->add_child(make_readout_label("ENERGY", "hud_label"));
    energy_value_label = {.label = make_readout_label("0", "hud_value"), .floor_digits = value_digit_floor()};
    group.row->add_child(energy_value_label.label);
    // The ceiling reads as a denominator on the number it constrains, the same way the wave counter shows its
    // total. Hidden on an uncapped match, and on a capped one until the reserve has actually fallen to the cap.
    energy_cap_label = make_readout_label("", "hud_wave_total");
    energy_cap_label->set_name("EnergyCap");
    energy_cap_label->set_visible(false);
    group.row->add_child(energy_cap_label);
    plate->add_child(group.row);
}

void HUD::build_info_plate() {
    PanelContainer *plate = build_plate("InfoPlate", "hud_tag", Control::PRESET_CENTER_TOP);
    info_plate = plate;

    // Level, wave and score sit on one line; the wide gap between groups is what keeps them legible as
    // three separate readings rather than one run-on string.
    auto *row = memnew(BoxContainer);
    info_row = row;
    row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    row->add_theme_constant_override("separation", UiThemeProvider::spacing("xl"));
    plate->add_child(row);

    // The level has no label: the name is the reading, and the flag already says what kind of reading it is.
    level_group = make_readout("level").row;
    level_group->set_name("LevelGroup");
    level_label = make_readout_label("", "hud_level");
    level_group->add_child(level_label);
    row->add_child(level_group);

    stats_row = memnew(BoxContainer);
    stats_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    stats_row->add_theme_constant_override("separation", UiThemeProvider::spacing("xl"));
    row->add_child(stats_row);

    const ReadoutRow wave_group = make_readout("wave");
    wave_group.row->add_child(make_readout_label("WAVE", "hud_label"));
    // A wave counter has no floor worth reserving: it starts at one digit and only ever widens if a level runs long.
    wave_current_label = {.label = make_readout_label("1", "hud_wave")};
    wave_group.row->add_child(wave_current_label.label);
    wave_total_label = make_readout_label("/ 3", "hud_wave_total");
    wave_total_label->set_name("WaveTotal");
    wave_group.row->add_child(wave_total_label);
    stats_row->add_child(wave_group.row);

    // Supply sits between the wave and the score: it is the reading the player checks before every deployment, and
    // on an uncapped level it is not there at all. The squad mark is the recruit figure without its plus: the plus
    // reads as "add a unit type" on the upgrade cards that use `recruit`, and as noise on a live count of how many
    // units are standing.
    const ReadoutRow supply_readout = make_readout("squad");
    supply_group = supply_readout.row;
    supply_group->set_name("SupplyGroup");
    supply_medallion = supply_readout.medallion;
    supply_group->add_child(make_readout_label("SUPPLY", "hud_label"));
    supply_current_label = {.label = make_readout_label("0", "hud_wave")};
    supply_group->add_child(supply_current_label.label);
    supply_cap_label = make_readout_label("", "hud_wave_total");
    supply_cap_label->set_name("SupplyCap");
    supply_group->add_child(supply_cap_label);
    supply_group->set_visible(false);
    stats_row->add_child(supply_group);

    const ReadoutRow score_group = make_readout("score");
    score_group.row->add_child(make_readout_label("SCORE", "hud_label"));
    score_label = {.label = make_readout_label("0", "hud_score"), .floor_digits = value_digit_floor()};
    score_group.row->add_child(score_label.label);
    stats_row->add_child(score_group.row);
}

void HUD::build_integrity_plate() {
    PanelContainer *plate = build_plate("IntegrityPlate", "hud_pod", Control::PRESET_TOP_RIGHT);
    integrity_plate = plate;

    const ReadoutRow group = make_readout("integrity");
    integrity_medallion = group.medallion;
    integrity_medallion.plate->set_name("IntegrityMedallion");
    group.row->add_child(make_readout_label("INTEGRITY", "hud_label"));

    integrity_meter = memnew(HudIntegrityMeter);
    integrity_meter->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
    group.row->add_child(integrity_meter);
    plate->add_child(group.row);
}

void HUD::collect_readout_sizes(Node *node) {
    if (auto *label = Object::cast_to<Label>(node); label != nullptr && label->get_name() != StringName("IntegrityPercentage")) {
        const String text = label->get_text();
        const bool caption = text == "ENERGY" || text == "WAVE" || text == "SUPPLY" || text == "SCORE" || text == "INTEGRITY";
        readout_labels.push_back({.label = label, .outline = label->get_theme_constant("outline_size"), .hide_in_compact = caption});
    }
    if (auto *box = Object::cast_to<BoxContainer>(node)) {
        readout_gaps.push_back({.box = box, .separation = box->get_theme_constant("separation")});
    }
    if (auto *panel = Object::cast_to<Panel>(node)) {
        readout_icons.push_back(panel);
    }
    for (int index = 0; index < node->get_child_count(); ++index) {
        collect_readout_sizes(node->get_child(index));
    }
}

void HUD::apply_readout_sizing() {
    const float scale = sizing.readout_scale;
    for (const auto &item : readout_labels) {
        item.label->add_theme_constant_override("outline_size", static_cast<int>(std::round(static_cast<float>(item.outline) * scale)));
        if (item.hide_in_compact) {
            item.label->set_visible(!sizing.compact);
        }
    }
    for (const auto &item : readout_gaps) {
        item.box->add_theme_constant_override("separation", static_cast<int>(std::round(static_cast<float>(item.separation) * scale)));
    }
    if (sizing.responsive) {
        const int group_gap = static_cast<int>(std::round(static_cast<float>(UiThemeProvider::metric("mobile_hud_group_gap", 8)) / sizing.pixels_per_unit));
        info_row->add_theme_constant_override("separation", group_gap);
        stats_row->add_theme_constant_override("separation", group_gap);
    }
    for (Control *icon : readout_icons) {
        const float size = UiThemeProvider::metric("hud_icon_size", 38) * scale;
        icon->set_custom_minimum_size({size, size});
    }
    energy_plate->set_custom_minimum_size({0.0F, UiThemeProvider::metric("hud_plate_height", 64) * scale});
    integrity_plate->set_custom_minimum_size(energy_plate->get_custom_minimum_size());
    info_plate->set_custom_minimum_size(energy_plate->get_custom_minimum_size());
    energy_plate->add_theme_stylebox_override("panel", scaled_surface("hud_pod", scale));
    integrity_plate->add_theme_stylebox_override("panel", scaled_surface("hud_pod", scale));
    info_plate->add_theme_stylebox_override("panel", scaled_surface("hud_tag", scale));
    level_label->set_clip_text(sizing.responsive);
    level_label->set_text_overrun_behavior(sizing.responsive ? TextServer::OVERRUN_TRIM_ELLIPSIS : TextServer::OVERRUN_NO_TRIMMING);
    // A clipped Label's minimum width is zero; give only this label a bounded, measured reservation.
    level_label->set_custom_minimum_size({0.0F, 0.0F});
    if (sizing.responsive) {
        const Ref<Font> font = level_label->get_theme_font("font");
        const float width = font->get_string_size(level_label->get_text(), HORIZONTAL_ALIGNMENT_LEFT, -1, UiThemeProvider::font_size("body")).x;
        level_label->set_custom_minimum_size({std::min(width, sizing.level_width), 0.0F});
    }
    integrity_meter->set_layout(scale, sizing.integrity_width);
    for (HudValueLabel *value : {&energy_value_label, &wave_current_label, &supply_current_label, &score_label}) {
        value->remeasure();
    }
}

void HUD::layout_ui() {
    if (card_container == nullptr) {
        return;
    }
    layout_dirty = false;
    info_row->set_vertical(false);
    stats_row->set_vertical(false);
    const float available_width = viewport_metrics.width - (2.0F * sizing.margin);
    const float top_available = available_width - (viewport_metrics.overlay_width / sizing.pixels_per_unit) - sizing.gap;
    if (sizing.responsive && !sizing.compact &&
        energy_plate->get_combined_minimum_size().x + info_plate->get_combined_minimum_size().x + integrity_plate->get_combined_minimum_size().x +
                (2.0F * sizing.gap) >
            top_available) {
        // Wave, supply, and score already have distinct marks. Drop their redundant captions before moving
        // the middle plate onto another row; keep the level name and the outer economy/integrity captions.
        for (const auto &item : readout_labels) {
            if (item.hide_in_compact && info_plate->is_ancestor_of(item.label)) {
                item.label->hide();
            }
        }
    }
    if (sizing.responsive && info_plate->get_combined_minimum_size().x > available_width) {
        info_row->set_vertical(true);
        if (stats_row->get_combined_minimum_size().x > available_width) {
            stats_row->set_vertical(true);
        }
    }
    const float card_gap = sizing.responsive ? sizing.gap : static_cast<float>(UiThemeProvider::spacing("md"));
    const float tray_available = std::max(1.0F, available_width - (sizing.pause_width > 0.0F ? sizing.pause_width + sizing.gap : 0.0F));
    std::vector<HudRect> card_sizes;
    card_sizes.reserve(deploy_cards.size());
    for (const auto &card : deploy_cards) {
        card_sizes.push_back(minimum_rect(card.button));
    }
    const auto arrangement = arrange_hud_cards(card_sizes, tray_available, card_gap, sizing.wrap_cards);
    for (size_t index = 0; index < deploy_cards.size(); ++index) {
        set_rect(deploy_cards[index].button, arrangement.cards[index]);
    }
    card_container->set_custom_minimum_size({arrangement.width, arrangement.height});
    if (sizing.wrap_cards) {
        card_tray->set_h_scroll(0);
    }
    const HudPlateSizes plates{.energy = minimum_rect(energy_plate), .info = minimum_rect(info_plate), .integrity = minimum_rect(integrity_plate)};
    HudPlacement placement = place_hud(viewport_metrics, sizing, plates, arrangement.width, arrangement.height);
    card_tray->set_horizontal_scroll_mode(placement.scroll_cards && !sizing.wrap_cards ? ScrollContainer::SCROLL_MODE_AUTO
                                                                                       : ScrollContainer::SCROLL_MODE_DISABLED);
    if (placement.scroll_cards) {
        // Reserve the scrollbar's actual themed height; it remains visible as an overflow affordance.
        placement.tray.height += card_tray->get_h_scroll_bar()->get_combined_minimum_size().y;
        placement.tray.y = viewport_metrics.height - sizing.margin - placement.tray.height;
    }
    set_rect(energy_plate, placement.energy);
    set_rect(info_plate, placement.info);
    set_rect(integrity_plate, placement.integrity);
    card_tray->set_visible(!deploy_cards.empty());
    set_rect(card_tray, placement.tray);
    card_scroll_hint->set_visible(placement.scroll_cards && sizing.responsive);
    if (placement.scroll_cards && sizing.responsive) {
        const godot::Vector2 hint = card_scroll_hint->get_combined_minimum_size();
        card_scroll_hint->set_position({placement.tray.x + placement.tray.width - hint.x, placement.tray.y - hint.y - sizing.gap});
    }
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
    energy_cap_label->set_text(to_godot_string(model.energy_cap.cap_text));
    energy_cap_label->set_visible(model.energy_cap.visible);

    supply_current_label.set_value(to_godot_string(model.supply_text));
    supply_cap_label->set_text(to_godot_string(model.supply.cap_text));
    supply_group->set_visible(model.supply.visible);
    render_supply_state(model.supply);

    wave_current_label.set_value(to_godot_string(model.wave.current_text));
    score_label.set_value(to_godot_string(model.score_text));
    wave_total_label->set_text(to_godot_string(model.wave.total_text));
    wave_total_label->set_visible(model.wave.total_visible);

    const bool level_changed = level_label->get_text() != to_godot_string(model.level_text);
    level_label->set_text(to_godot_string(model.level_text));
    level_group->set_visible(model.level_visible);

    render_integrity(model.integrity);
    render_deploy_cards(model.deploy_cards);
    if (level_changed && sizing.responsive) {
        apply_readout_sizing();
    }
    layout_dirty = true;
}

void HUD::render_supply_state(const HudCapModel &supply) {
    // A full line is the one state the player has to read at a glance: every deploy card is refused until something
    // dies, and the difference between "waiting for energy" and "waiting for a casualty" is the whole decision.
    // Re-tinting rebuilds a style box and reloads the mark, so it only happens when the state actually flips.
    if (supply_at_cap.has_value() && *supply_at_cap == supply.at_cap) {
        return;
    }
    supply_at_cap = supply.at_cap;

    const godot::Color color = UiThemeProvider::color(supply.at_cap ? "state_warning" : "accent");
    apply_icon_medallion(supply_medallion, theme_icon("squad"), color);
    supply_current_label.label->add_theme_color_override("font_color", color);
    supply_cap_label->add_theme_color_override("font_color", color);
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
            // Let pointer motion reach the tray so swiping over a card scrolls and cancels its pending click.
            button->set_mouse_filter(Control::MOUSE_FILTER_PASS);
            button->connect("pressed", callable_mp(this, &HUD::on_card_pressed).bind(to_godot_string(card_model.card.unit_id)));
            button->connect("focus_entered", callable_mp(this, &HUD::on_deploy_card_focused).bind(button));
            card_container->add_child(button);
            // Measure after entering the tree, where the shared browser typography is inherited.
            DeployCardPresenter::apply_sizing(button, sizing);
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

void HUD::on_deploy_scroll_started() {
    // A touch press focuses a card before ScrollContainer recognises the drag. Drop that focus on a swipe so
    // the next keyboard/pad selection can reveal its card again. Ordinary taps keep their focus.
    for (const DeployCardUI &card : deploy_cards) {
        if (card.button->has_focus()) {
            card.button->release_focus();
        }
    }
}

void HUD::on_deploy_card_focused(Control *button) {
    // Touch is translated into a left mouse press by Godot. Revealing that pressed card would move the tray
    // underneath a swipe before scrolling starts; only keyboard/pad navigation needs automatic revealing.
    if (!Input::get_singleton()->is_mouse_button_pressed(MOUSE_BUTTON_LEFT)) {
        card_tray->ensure_control_visible(button);
    }
}

void HUD::update_core_resource(int value) {
    hud_input_.energy = value;
    refresh();
}

void HUD::set_energy_cap(int energy_cap) {
    hud_input_.energy_cap = energy_cap;
    refresh();
}

void HUD::update_supply(int used, int cap, bool energy_ceiling_engaged) {
    hud_input_.supply_used = used;
    hud_input_.supply_cap = cap;
    hud_input_.energy_ceiling_engaged = energy_ceiling_engaged;
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
                                  .on_endless = callable_mp(this, &HUD::on_endless_pressed),
                                  .on_retry = callable_mp(this, &HUD::on_retry_pressed).bind(to_godot_string(summary.current_level_id)),
                                  .on_campaign = callable_mp(this, &HUD::on_campaign_pressed),
                                  .on_select_upgrade = callable_mp(this, &HUD::on_upgrade_card_pressed),
                              });

    score_screen_overlay = view.overlay;
    score_screen_panel = view.panel;
}

void HUD::on_next_level_pressed(const String &level_id) { emit_signal("score_screen_next_level", level_id); }

void HUD::on_retry_pressed(const String &level_id) { emit_signal("score_screen_retry", level_id); }

void HUD::on_endless_pressed() { emit_signal("score_screen_endless"); }

void HUD::on_campaign_pressed() { emit_signal("score_screen_campaign"); }

void HUD::on_upgrade_card_pressed(const String &upgrade_id) { emit_signal("score_screen_upgrade_selected", upgrade_id); }

} // namespace defn
