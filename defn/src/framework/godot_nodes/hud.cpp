// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "hud.h"
#include "deploy_card_presenter.h"
#include "deploy_tray.h"
#include "godot_color.h"
#include "godot_string.h"
#include "score_screen_view.h"
#include "ui_sfx_player.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"
#include <algorithm>
#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/h_flow_container.hpp>
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
    ADD_SIGNAL(MethodInfo("score_screen_endless"));
    ADD_SIGNAL(MethodInfo("score_screen_campaign"));
    ADD_SIGNAL(MethodInfo("score_screen_upgrade_selected", PropertyInfo(Variant::STRING, "upgrade_id")));
}

void HUD::_ready() {
    set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    set_mouse_filter(MOUSE_FILTER_IGNORE);
    UiThemeProvider::install(get_tree());
    UiSfxPlayer::install(this);
    build_ui();
}

void HUD::build_ui() {
    match_controls_ = memnew(Control);
    match_controls_->set_name("MatchControls");
    match_controls_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    add_child(match_controls_);
    build_energy_plate();
    build_info_plate();
    build_integrity_plate();

    // ==========================================================
    // Deploy card container (bottom center)
    // ==========================================================
    tray_ = memnew(DeployTray);
    tray_->set_name("DeployTray");
    match_controls_->add_child(tray_);

    metrics_ = make_surface("hud_pod");
    metrics_->set_name("CompactMetrics");
    match_controls_->add_child(metrics_);
    auto *row = memnew(HBoxContainer);
    metrics_->add_child(row);
    flow_ = memnew(HFlowContainer);
    flow_->set_h_size_flags(SIZE_EXPAND_FILL);
    flow_->add_theme_constant_override("h_separation", UiThemeProvider::spacing("sm"));
    flow_->add_theme_constant_override("v_separation", UiThemeProvider::spacing("sm"));
    row->add_child(flow_);
    single_row_ = memnew(HBoxContainer);
    single_row_->set_name("SingleRowMetrics");
    single_row_->hide();
    row->add_child(single_row_);
    pause_button_ = make_button("||", "pause");
    pause_button_->set_accessibility_name("Pause");
    pause_button_->set_v_size_flags(SIZE_SHRINK_CENTER);
    row->add_child(pause_button_);
    metrics_->hide();

    for (auto *group : reading_groups()) {
        group->connect("minimum_size_changed", callable_mp(this, &HUD::invalidate_measurement));
        group->connect("visibility_changed", callable_mp(this, &HUD::invalidate_measurement));
    }
    apply_appearance(UiThemeProvider::appearance(UiThemeContext::Default));
    refresh();
}

std::array<Control *, 6> HUD::reading_groups() const { return {energy_group_, integrity_group_, supply_group, wave_group_, score_group_, level_group}; }
void HUD::invalidate_measurement() {
    measure_dirty_ = true;
    if (invalidate_layout_) {
        invalidate_layout_();
    }
}
void HUD::apply_appearance(const UiAppearance &appearance) {
    if (appearance_ready_ && appearance_.context == appearance.context && appearance_.revision == appearance.revision) {
        return;
    }
    appearance_ = appearance;
    desktop_reference_ = appearance.context == UiThemeContext::DesktopMatch;
    phone_landscape_ = appearance.context == UiThemeContext::PhoneLandscape;
    match_controls_->set_theme(desktop_reference_ ? appearance.theme : UiThemeProvider::theme());
    prepare_metrics();
    tray_->apply_appearance(appearance);
    appearance_ready_ = true;
    invalidate_measurement();
}

void HUD::set_pause_action(const Callable &action) { pause_button_->connect("pressed", action); }

HudLayoutMetrics HUD::measure_single_row() const {
    const auto &theme = *appearance_.data;
    HudLayoutMetrics measured;
    const auto inset = static_cast<float>(2 * theme.spacing.sm);
    int groups = 0;
    for (auto *group : {energy_group_, integrity_group_, supply_group, wave_group_, score_group_}) {
        if (group->is_visible()) {
            const auto minimum = group->get_combined_minimum_size();
            measured.single_row.width += minimum.x;
            measured.single_row.height = std::max(measured.single_row.height, minimum.y);
            ++groups;
        }
    }
    measured.single_row.width += inset + static_cast<float>(std::max(0, groups - 1) * theme.metric("hud_group_gap", 8));
    measured.single_row.height += inset;
    measured.flow_height = measured.single_row.height;
    return measured;
}

HudLayoutMetrics HUD::measure_layout(float width) {
    if (!measure_dirty_ && cached_measure_width_ == width) {
        return cached_measurement_;
    }
    cached_measure_width_ = width;
    measure_dirty_ = false;
    if (phone_landscape_) {
        cached_measurement_ = measure_single_row();
        return cached_measurement_;
    }
    const auto &data = appearance_.data->responsive;
    const float inset = 2 * static_cast<float>(appearance_.data->spacing.sm);
    const float available = std::max(1.0F, width - pause_button_->get_combined_minimum_size().x - data.gap - inset);
    float occupied = 0;
    float line_height = 0;
    int rows = 1;
    for (auto *group : reading_groups()) {
        if (!group->is_visible()) {
            continue;
        }
        const auto minimum = group->get_combined_minimum_size();
        if (occupied > 0 && occupied + data.gap + minimum.x > available) {
            ++rows;
            occupied = 0;
        }
        occupied += minimum.x + data.gap;
        line_height = std::max(line_height, minimum.y);
    }
    const float content = (static_cast<float>(rows) * line_height) + (static_cast<float>(rows - 1) * data.gap);
    HudLayoutMetrics measured;
    measured.flow_height = std::max(content, pause_button_->get_combined_minimum_size().y) + inset;
    float info_width = 0;
    int info_groups = 0;
    for (auto *group : {level_group, wave_group_, supply_group, score_group_}) {
        if (group->is_visible()) {
            info_width += group->get_combined_minimum_size().x;
            ++info_groups;
        }
    }
    measured.instruments_width = energy_group_->get_combined_minimum_size().x + integrity_group_->get_combined_minimum_size().x + info_width + 2 * inset +
                                 2 * static_cast<float>(appearance_.data->spacing.md) +
                                 static_cast<float>(std::max(0, info_groups - 1) * appearance_.data->metric("hud_group_gap", 12)) + 2 * data.gap;
    measured.instruments_height = static_cast<float>(appearance_.data->metric("hud_plate_height", 64));
    measured.primary_gutter = {.width = 0, .height = inset};
    for (auto *group : {energy_group_, integrity_group_, supply_group, wave_group_}) {
        if (group->is_visible()) {
            const auto minimum = group->get_combined_minimum_size();
            measured.primary_gutter.width = std::max(measured.primary_gutter.width, minimum.x + inset);
            measured.primary_gutter.height += minimum.y + data.gap;
        }
    }
    measured.primary_gutter.height += pause_button_->get_combined_minimum_size().y;
    measured.secondary_gutter = {.width = 0, .height = inset};
    for (auto *group : {level_group, score_group_}) {
        if (group->is_visible()) {
            const auto minimum = group->get_combined_minimum_size();
            measured.secondary_gutter.width = std::max(measured.secondary_gutter.width, minimum.x + inset);
            measured.secondary_gutter.height += minimum.y + data.gap;
        }
    }
    measured.secondary_gutter.height -= data.gap;
    cached_measurement_ = measured;
    return measured;
}

void HUD::prepare_metrics() {
    const auto &data = *appearance_.data;
    metrics_->set_theme(appearance_.theme);
    const auto icon_size = static_cast<float>(data.metric("hud_icon_size", 38));
    for (auto *group : reading_groups()) {
        group->set_theme(appearance_.theme);
        group->add_theme_constant_override("separation", data.spacing.sm);
        if (auto *icon = Object::cast_to<Control>(group->get_child(0)); icon != nullptr) {
            icon->set_custom_minimum_size({icon_size, icon_size});
        }
    }
    energy_value_label.floor_digits = data.metric("hud_min_value_digits", 3);
    score_label.floor_digits = energy_value_label.floor_digits;
    {
        for (auto *value : {&energy_value_label, &wave_current_label, &supply_current_label, &score_label}) {
            const auto text = value->label->get_text();
            value->reserved_digits = 0;
            value->label->set_custom_minimum_size({0, 0});
            value->set_value(text);
        }
    }
    integrity_meter->apply_appearance(data);
    level_group->set_visible(!phone_landscape_ && HudPresenter::build(hud_input_).level_visible);
    level_label->set_clip_text(!desktop_reference_);
    level_label->set_text_overrun_behavior(desktop_reference_ ? TextServer::OVERRUN_NO_TRIMMING : TextServer::OVERRUN_TRIM_ELLIPSIS);
    reserve_level_width();
    render_integrity(HudPresenter::build(hud_input_).integrity);
}

void HUD::reserve_level_width() {
    const auto font = level_label->get_theme_font("font");
    const float level_width = font->get_string_size(level_label->get_text(), HORIZONTAL_ALIGNMENT_LEFT, -1, level_label->get_theme_font_size("font_size")).x;
    level_label->set_custom_minimum_size(
        {desktop_reference_ ? 0.0F : std::min(level_width, static_cast<float>(appearance_.data->metric("hud_level_max_width", 180))), 0});
}

bool HUD::configure_match_controls(const MatchLayout &layout) {
    const bool phone_landscape = layout.hud == HudArrangement::SingleRow;
    const bool changed_presentation = desktop_reference_ != layout.desktop_reference || phone_landscape_ != phone_landscape;
    desktop_reference_ = layout.desktop_reference;
    phone_landscape_ = phone_landscape;
    match_controls_->set_position(desktop_reference_ ? godot::Vector2(layout.battlefield.x, layout.battlefield.y) : godot::Vector2{});
    match_controls_->set_size(desktop_reference_ ? godot::Vector2(layout.reference.width, layout.reference.height) : get_size());
    const float scale = desktop_reference_ ? layout.battlefield.width / layout.reference.width : 1;
    match_controls_->set_scale({scale, scale});

    level_label->set_clip_text(!desktop_reference_);
    level_label->set_text_overrun_behavior(desktop_reference_ ? TextServer::OVERRUN_NO_TRIMMING : TextServer::OVERRUN_TRIM_ELLIPSIS);
    if (desktop_reference_) {
        level_label->set_custom_minimum_size({0, 0});
    }
    return changed_presentation;
}

void HUD::reflow_single_row() {
    int index = 0;
    for (auto *group : reading_groups()) {
        if (group->get_parent() != single_row_) {
            group->reparent(single_row_);
        }
        single_row_->move_child(group, index++);
    }
    level_group->hide();
    if (pause_button_->get_parent() != match_controls_) {
        pause_button_->reparent(match_controls_);
    }
    // Pause keeps a full touch target without increasing the height or offsetting the centered readings.
    pause_button_->set_theme(UiThemeProvider::theme());
    single_row_->add_theme_constant_override("separation", appearance_.data->metric("hud_group_gap", 8));
    single_row_->show();
    flow_->hide();
    pause_button_->show();
    energy_plate_->hide();
    integrity_plate_->hide();
    info_plate_->hide();
    metrics_->show();
}

void HUD::reflow_groups(HudArrangement arrangement) {
    const auto &theme = *appearance_.data;
    arrangement_ = arrangement;
    if (arrangement == HudArrangement::SingleRow) {
        reflow_single_row();
        return;
    }
    single_row_->hide();
    flow_->show();
    const bool instruments = arrangement == HudArrangement::Instruments;
    const bool gutters = arrangement == HudArrangement::Gutters;
    show_pause_ = !instruments;
    pause_button_->set_visible(show_pause_);
    auto move = [](Control *control, Node *parent) {
        if (control->get_parent() != parent) {
            control->reparent(parent);
        }
    };
    move(energy_group_, instruments ? static_cast<Node *>(energy_plate_) : flow_);
    move(integrity_group_, instruments ? static_cast<Node *>(integrity_plate_) : flow_);
    move(supply_group, instruments ? static_cast<Node *>(info_row_) : flow_);
    move(wave_group_, instruments ? static_cast<Node *>(info_row_) : flow_);
    move(level_group, instruments || gutters ? static_cast<Node *>(info_row_) : flow_);
    move(score_group_, instruments || gutters ? static_cast<Node *>(info_row_) : flow_);
    move(pause_button_, gutters ? static_cast<Node *>(flow_) : flow_->get_parent());
    info_row_->set_vertical(gutters);
    info_row_->add_theme_constant_override("separation", instruments ? theme.metric("hud_group_gap", 12) : UiThemeProvider::spacing("sm"));
    if (instruments) {
        info_row_->move_child(level_group, 0);
        info_row_->move_child(wave_group_, 1);
        info_row_->move_child(supply_group, 2);
        info_row_->move_child(score_group_, 3);
    } else {
        flow_->move_child(energy_group_, 0);
        flow_->move_child(integrity_group_, 1);
        flow_->move_child(supply_group, 2);
        flow_->move_child(wave_group_, 3);
        if (gutters) {
            flow_->move_child(pause_button_, 4);
        } else {
            flow_->move_child(score_group_, 4);
            flow_->move_child(level_group, 5);
        }
    }
    energy_plate_->set_visible(instruments);
    integrity_plate_->set_visible(instruments);
    info_plate_->set_visible(instruments || gutters);
    metrics_->set_visible(!instruments);
    info_plate_->set_theme_type_variation(UiThemeProvider::panel_variation(instruments ? "hud_tag" : "hud_pod"));
}

void HUD::place_match_controls(const MatchLayout &layout) {
    const bool instruments = layout.hud == HudArrangement::Instruments;
    const bool gutters = layout.hud == HudArrangement::Gutters;
    auto place = [](Control *control, UiRect rectangle) {
        control->set_anchors_and_offsets_preset(PRESET_TOP_LEFT);
        control->set_position({rectangle.x, rectangle.y});
        control->set_size({rectangle.width, rectangle.height});
    };
    place(metrics_, layout.metrics);
    metrics_->set_scale({layout.metrics_scale, layout.metrics_scale});
    metrics_->set_size({layout.metrics.width / layout.metrics_scale, layout.metrics.height / layout.metrics_scale});
    if (phone_landscape_) {
        place(pause_button_, layout.pause);
    }
    if (desktop_reference_) {
        const auto &theme = *appearance_.data;
        const auto margin = static_cast<float>(theme.metric("hud_margin", 24));
        const auto height = static_cast<float>(theme.metric("hud_plate_height", 64));
        auto anchor = [margin, height](Control *plate, LayoutPreset preset, float edge, GrowDirection growth) {
            plate->set_custom_minimum_size({0, height});
            plate->set_anchors_preset(preset);
            plate->set_h_grow_direction(growth);
            plate->set_v_grow_direction(GROW_DIRECTION_END);
            plate->set_offset(SIDE_LEFT, edge);
            plate->set_offset(SIDE_RIGHT, edge);
            plate->set_offset(SIDE_TOP, margin);
            plate->set_offset(SIDE_BOTTOM, margin);
        };
        anchor(energy_plate_, PRESET_TOP_LEFT, margin, GROW_DIRECTION_END);
        anchor(info_plate_, PRESET_CENTER_TOP, 0, GROW_DIRECTION_BOTH);
        anchor(integrity_plate_, PRESET_TOP_RIGHT, -margin, GROW_DIRECTION_BEGIN);
    } else if (instruments) {
        const float left = energy_plate_->get_combined_minimum_size().x;
        const float right = integrity_plate_->get_combined_minimum_size().x;
        const float gap = appearance_.data->responsive.gap;
        place(energy_plate_, {.x = layout.metrics.x, .y = layout.metrics.y, .width = left, .height = layout.metrics.height});
        place(integrity_plate_, {.x = layout.metrics.x + layout.metrics.width - right, .y = layout.metrics.y, .width = right, .height = layout.metrics.height});
        place(info_plate_, {.x = layout.metrics.x + left + gap,
                            .y = layout.metrics.y,
                            .width = layout.metrics.width - left - right - (2 * gap),
                            .height = layout.metrics.height});
    } else if (gutters) {
        place(info_plate_, layout.details);
    }
}

void HUD::apply_layout(const MatchLayout &layout) {
    const bool changed_presentation = configure_match_controls(layout);
    if (arrangement_ != layout.hud || !reflowed_) {
        reflow_groups(layout.hud);
        reflowed_ = true;
    }
    place_match_controls(layout);
    metrics_width_ = layout.metrics.width;
    tray_->apply_layout(layout);
    if (changed_presentation) {
        for (auto &card : deploy_cards) {
            card.enabled.reset();
        }
        refresh();
    }
}

PanelContainer *HUD::build_plate(const char *name, std::string_view surface, Control::LayoutPreset preset) {
    auto *plate = make_surface(surface);
    plate->set_name(name);
    anchor_hud_pod(plate, preset);
    match_controls_->add_child(plate);
    return plate;
}

void HUD::build_energy_plate() {
    PanelContainer *plate = build_plate("EnergyPlate", "hud_pod", Control::PRESET_TOP_LEFT);
    energy_plate_ = plate;

    const ReadoutRow group = make_readout("energy");
    energy_group_ = group.row;
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
    info_plate_ = plate;

    // Level, wave and score sit on one line; the wide gap between groups is what keeps them legible as
    // three separate readings rather than one run-on string.
    auto *row = memnew(BoxContainer);
    info_row_ = row;
    row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    row->add_theme_constant_override("separation", UiThemeProvider::spacing("xl"));
    plate->add_child(row);

    // The level has no label: the name is the reading, and the flag already says what kind of reading it is.
    level_group = make_readout("level").row;
    level_group->set_name("LevelGroup");
    level_label = make_readout_label("", "hud_level");
    level_label->set_clip_text(true);
    level_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
    level_group->add_child(level_label);
    row->add_child(level_group);

    const ReadoutRow wave_group = make_readout("wave");
    wave_group_ = wave_group.row;
    wave_group.row->add_child(make_readout_label("WAVE", "hud_label"));
    // A wave counter has no floor worth reserving: it starts at one digit and only ever widens if a level runs long.
    wave_current_label = {.label = make_readout_label("1", "hud_wave")};
    wave_group.row->add_child(wave_current_label.label);
    wave_total_label = make_readout_label("/ 3", "hud_wave_total");
    wave_total_label->set_name("WaveTotal");
    wave_group.row->add_child(wave_total_label);
    row->add_child(wave_group.row);

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
    row->add_child(supply_group);

    const ReadoutRow score_group = make_readout("score");
    score_group_ = score_group.row;
    score_group.row->add_child(make_readout_label("SCORE", "hud_label"));
    score_label = {.label = make_readout_label("0", "hud_score"), .floor_digits = value_digit_floor()};
    score_group.row->add_child(score_label.label);
    row->add_child(score_group.row);
}

void HUD::build_integrity_plate() {
    PanelContainer *plate = build_plate("IntegrityPlate", "hud_pod", Control::PRESET_TOP_RIGHT);
    integrity_plate_ = plate;

    const ReadoutRow group = make_readout("integrity");
    integrity_group_ = group.row;
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
    if (tray_ != nullptr) {
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

    const auto level = to_godot_string(model.level_text);
    if (level_label->get_text() != level) {
        level_label->set_text(level);
        reserve_level_width();
    }
    level_group->set_visible(model.level_visible && !phone_landscape_);

    render_integrity(model.integrity);
    pause_button_->set_tooltip_text("Pause\n" + to_godot_string(model.level_text) + "\nSCORE " + to_godot_string(model.score_text));
    level_label->set_tooltip_text(to_godot_string(model.level_text));
    render_deploy_cards(model.deploy_cards);
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
    if (tray_ == nullptr) {
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
            auto *button = tray_->add_card(card_model.card);
            button->connect("pressed", callable_mp(this, &HUD::on_card_pressed).bind(to_godot_string(card_model.card.unit_id)));
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
        if (!*card.enabled) {
            const auto &theme = *appearance_.data;
            const float value = static_cast<float>(theme.metric("deploy_disabled_value_percent", 50)) / 100;
            const float alpha = static_cast<float>(theme.metric("deploy_disabled_alpha_percent", 70)) / 100;
            card.button->set_modulate(godot::Color(value, value, value, alpha));
        }
    }
}

void HUD::clear_deploy_cards() {
    tray_->clear_cards();
    deploy_cards.clear();
}

void HUD::on_card_pressed(const String &unit_type) { emit_signal("deploy_requested", unit_type); }

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
                              },
                              context_);

    score_screen_overlay = view.overlay;
    score_screen_panel = view.panel;
}

void HUD::on_next_level_pressed(const String &level_id) { emit_signal("score_screen_next_level", level_id); }

void HUD::on_retry_pressed(const String &level_id) { emit_signal("score_screen_retry", level_id); }

void HUD::on_endless_pressed() { emit_signal("score_screen_endless"); }

void HUD::on_campaign_pressed() { emit_signal("score_screen_campaign"); }

void HUD::on_upgrade_card_pressed(const String &upgrade_id) { emit_signal("score_screen_upgrade_selected", upgrade_id); }

} // namespace defn
