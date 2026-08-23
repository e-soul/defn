// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "campaign_map_node_view.h"

#include "campaign_preview_view.h"
#include "godot_string.h"
#include "icon_medallion.h"
#include "ui_theme_provider.h"

#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/style_box_empty.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

using namespace godot;
using GColor = godot::Color;
using GVector2 = godot::Vector2;

namespace {

/// Maps the node state onto its `medallions` entry in `ui_theme.json`, which owns both the mark and the node accent colour.
std::string_view state_key(CampaignNodeState state) {
    switch (state) {
    case CampaignNodeState::COMPLETED:
        return "completed";
    case CampaignNodeState::AVAILABLE:
        return "available";
    case CampaignNodeState::FRONTIER:
        return "frontier";
    case CampaignNodeState::LOCKED:
        return "locked";
    }
    return "locked";
}

/// A selected node wears the same treatment as a selected card: the accent on the frame at the heavier border
/// width. The ring and the lift below are what a marker on a map needs on top of that to carry at map scale.
Ref<StyleBoxFlat> frame_style(const GColor &border, bool selected) {
    Ref<StyleBoxFlat> style = UiThemeProvider::surface("map_node");
    style->set_border_width_all(UiThemeProvider::shape(selected ? "border_width_strong" : "border_width"));
    style->set_border_color(selected ? UiThemeProvider::color("accent") : border);
    return style;
}

Ref<StyleBoxFlat> outline_style(const GColor &color, int width) {
    Ref<StyleBoxFlat> style = UiThemeProvider::surface("outline");
    style->set_border_color(color);
    style->set_border_width_all(width);
    return style;
}

/// Places a child centred inside `outer`, inset by the same margin on each axis.
void inset_within(Control *child, const GVector2 &outer, const GVector2 &inset) {
    child->set_position(inset);
    child->set_size(outer - (inset * 2.0F));
}

} // namespace

CampaignMapNodeView::CampaignMapNodeView() {
    const GVector2 node_size{UiThemeProvider::metric("map_node_width", 188), UiThemeProvider::metric("map_node_height", 134)};
    set_custom_minimum_size(node_size);
    set_size(node_size);
    set_mouse_filter(MOUSE_FILTER_PASS);

    // Three concentric boxes: the ring sits just inside the node, the postcard frame inside that, and the
    // preview inside the frame. Each inset comes from the theme so the whole marker scales together.
    const GVector2 ring_inset{UiThemeProvider::metric("map_node_ring_inset_x", 7), UiThemeProvider::metric("map_node_ring_inset_y", 4)};
    const GVector2 frame_inset{UiThemeProvider::metric("map_node_frame_inset_x", 12), UiThemeProvider::metric("map_node_frame_inset_y", 9)};
    const real_t preview_inset = UiThemeProvider::metric("map_node_preview_inset", 4);

    selection_ring_ = memnew(Panel);
    selection_ring_->set_name("SelectionRing");
    inset_within(selection_ring_, node_size, ring_inset);
    selection_ring_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    selection_ring_->set_visible(false);
    add_child(selection_ring_);

    frame_ = memnew(Panel);
    frame_->set_name("PostcardFrame");
    inset_within(frame_, node_size, frame_inset);
    frame_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    add_child(frame_);

    preview_ = memnew(CampaignPreviewView);
    inset_within(preview_, frame_->get_size(), {preview_inset, preview_inset});
    frame_->add_child(preview_);

    medallion_ = make_icon_medallion(UiThemeProvider::metric("medallion_size", 38));
    medallion_.plate->set_name("StateMedallion");
    medallion_.mark->set_name("StateMark");
    add_child(medallion_.plate);

    interaction_ = memnew(Button);
    interaction_->set_name("Interaction");
    interaction_->set_flat(true);
    interaction_->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    interaction_->set_focus_mode(FOCUS_NONE);
    interaction_->set_default_cursor_shape(CURSOR_POINTING_HAND);
    Ref<StyleBoxEmpty> empty_button_style;
    empty_button_style.instantiate();
    interaction_->add_theme_stylebox_override("normal", empty_button_style);
    interaction_->add_theme_stylebox_override("hover", empty_button_style);
    interaction_->add_theme_stylebox_override("pressed", empty_button_style);
    interaction_->connect("gui_input", callable_mp(this, &CampaignMapNodeView::on_gui_input));
    interaction_->connect("pressed", callable_mp(this, &CampaignMapNodeView::on_pressed));
    add_child(interaction_);
}

void CampaignMapNodeView::_bind_methods() {
    ADD_SIGNAL(MethodInfo("selected", PropertyInfo(Variant::STRING, "level_id")));
    ADD_SIGNAL(MethodInfo("activated", PropertyInfo(Variant::STRING, "level_id")));
}

void CampaignMapNodeView::configure(const CampaignMissionViewModel &mission, const Ref<Texture2D> &preview_texture) {
    mission_ = mission;
    preview_->configure(preview_texture, mission.preview.focus_x, mission.preview.focus_y, mission.preview.node_zoom);
    interaction_->set_tooltip_text(mission.state == CampaignNodeState::LOCKED ? to_godot_string(mission.unlock_requirement) : String());
    update_style();
}

void CampaignMapNodeView::set_selected(bool selected) {
    selected_ = selected;
    // A card lifts a step up the neutral ramp to show it is chosen; a marker on a map lifts by growing, since
    // it has artwork rather than a background to change.
    const real_t lift = UiThemeProvider::metric("map_node_selected_scale_percent", 104) / 100.0F;
    set_pivot_offset(get_size() * 0.5F);
    set_scale(selected ? GVector2(lift, lift) : GVector2(1.0F, 1.0F));
    set_z_index(selected ? 2 : 0);
    update_style();
}

void CampaignMapNodeView::on_gui_input(const Ref<InputEvent> &event) {
    if (!event.is_valid()) {
        return;
    }

    const auto *mouse = Object::cast_to<InputEventMouseButton>(event.ptr());
    const bool double_click = mouse != nullptr && mouse->get_button_index() == MOUSE_BUTTON_LEFT && mouse->is_pressed() && mouse->is_double_click();
    if (double_click) {
        emit_signal("selected", to_godot_string(mission_.level_id));
        if (mission_.state != CampaignNodeState::LOCKED) {
            emit_signal("activated", to_godot_string(mission_.level_id));
        }
    }
}

void CampaignMapNodeView::on_pressed() { emit_signal("selected", to_godot_string(mission_.level_id)); }

void CampaignMapNodeView::update_style() {
    const UiMedallionStyle &medallion = theme_medallion(state_key(mission_.state));
    const GColor color = UiThemeProvider::color(medallion.color_role);
    frame_->add_theme_stylebox_override("panel", frame_style(color, selected_));
    selection_ring_->add_theme_stylebox_override("panel", outline_style(color, UiThemeProvider::shape("border_width")));
    selection_ring_->set_visible(selected_);
    medallion_.plate->set_position({UiThemeProvider::metric("medallion_offset_x", -5), UiThemeProvider::metric("medallion_offset_y", -4)});
    apply_icon_medallion(medallion_, medallion, color);
    preview_->set_modulate(mission_.state == CampaignNodeState::LOCKED ? UiThemeProvider::color("locked_tint") : GColor(1, 1, 1, 1));
}

} // namespace defn
