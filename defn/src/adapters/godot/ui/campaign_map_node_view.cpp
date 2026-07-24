#include "campaign_map_node_view.h"

#include "campaign_preview_view.h"
#include "godot_string.h"

#include <godot_cpp/classes/style_box_empty.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

using namespace godot;
using GColor = godot::Color;
using GVector2 = godot::Vector2;

namespace {

GColor state_color(CampaignNodeState state) {
    switch (state) {
    case CampaignNodeState::COMPLETED:
        return {"5fcb9a"};
    case CampaignNodeState::AVAILABLE:
    case CampaignNodeState::FRONTIER:
        return {"f2be55"};
    case CampaignNodeState::LOCKED:
        return {"59646c"};
    }
    return {"59646c"};
}

String state_icon(CampaignNodeState state) {
    switch (state) {
    case CampaignNodeState::COMPLETED:
        return String::utf8("✓");
    case CampaignNodeState::AVAILABLE:
        return String::utf8("◇");
    case CampaignNodeState::FRONTIER:
        return String::utf8("◆");
    case CampaignNodeState::LOCKED:
        return String::utf8("🔒");
    }
    return "?";
}

Ref<StyleBoxFlat> frame_style(const GColor &border, bool selected) {
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(GColor("0a1118"));
    style->set_border_width_all(selected ? 5 : 3);
    style->set_border_color(selected ? GColor("f7e5a0") : border);
    style->set_corner_radius_all(5);
    style->set_shadow_color(GColor(0.0F, 0.0F, 0.0F, 0.75F));
    style->set_shadow_size(selected ? 12 : 8);
    return style;
}

Ref<StyleBoxFlat> outline_style(const GColor &color, int width) {
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(GColor(0.0F, 0.0F, 0.0F, 0.0F));
    style->set_border_color(color);
    style->set_border_width_all(width);
    style->set_corner_radius_all(8);
    return style;
}

Ref<StyleBoxFlat> medallion_style(const GColor &border) {
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(GColor("0a1118"));
    style->set_border_color(border);
    style->set_border_width_all(2);
    style->set_corner_radius_all(19);
    style->set_shadow_color(GColor(0.0F, 0.0F, 0.0F, 0.8F));
    style->set_shadow_size(5);
    return style;
}

} // namespace

CampaignMapNodeView::CampaignMapNodeView() {
    set_custom_minimum_size({188.0F, 134.0F});
    set_size({188.0F, 134.0F});
    set_mouse_filter(MOUSE_FILTER_PASS);

    selection_ring_ = memnew(Panel);
    selection_ring_->set_name("SelectionRing");
    selection_ring_->set_position({7.0F, 4.0F});
    selection_ring_->set_size({174.0F, 126.0F});
    selection_ring_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    selection_ring_->set_visible(false);
    add_child(selection_ring_);

    focus_ring_ = memnew(Panel);
    focus_ring_->set_name("FocusRing");
    focus_ring_->set_position({3.0F, 0.0F});
    focus_ring_->set_size({182.0F, 134.0F});
    focus_ring_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    focus_ring_->set_visible(false);
    add_child(focus_ring_);

    frame_ = memnew(Panel);
    frame_->set_name("PostcardFrame");
    frame_->set_position({12.0F, 9.0F});
    frame_->set_size({164.0F, 116.0F});
    frame_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    add_child(frame_);

    preview_ = memnew(CampaignPreviewView);
    preview_->set_position({4.0F, 4.0F});
    preview_->set_size({156.0F, 108.0F});
    frame_->add_child(preview_);

    medallion_ = memnew(Label);
    medallion_->set_name("StateMedallion");
    medallion_->set_position({-5.0F, -4.0F});
    medallion_->set_size({38.0F, 38.0F});
    medallion_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    medallion_->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
    medallion_->add_theme_font_size_override("font_size", 22);
    medallion_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    add_child(medallion_);

    interaction_ = memnew(Button);
    interaction_->set_name("Interaction");
    interaction_->set_flat(true);
    interaction_->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    interaction_->set_focus_mode(FOCUS_ALL);
    interaction_->set_default_cursor_shape(CURSOR_POINTING_HAND);
    Ref<StyleBoxEmpty> empty_button_style;
    empty_button_style.instantiate();
    interaction_->add_theme_stylebox_override("normal", empty_button_style);
    interaction_->add_theme_stylebox_override("hover", empty_button_style);
    interaction_->add_theme_stylebox_override("pressed", empty_button_style);
    interaction_->add_theme_stylebox_override("focus", empty_button_style);
    interaction_->connect("mouse_entered", callable_mp(this, &CampaignMapNodeView::on_pointer_entered));
    interaction_->connect("focus_entered", callable_mp(this, &CampaignMapNodeView::on_focus_entered));
    interaction_->connect("focus_exited", callable_mp(this, &CampaignMapNodeView::on_focus_exited));
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
    set_pivot_offset(get_size() * 0.5F);
    set_scale(selected ? GVector2(1.04F, 1.04F) : GVector2(1.0F, 1.0F));
    set_z_index(selected ? 2 : 0);
    update_style();
}

void CampaignMapNodeView::grab_node_focus() { interaction_->grab_focus(); }

void CampaignMapNodeView::on_pointer_entered() { emit_signal("selected", to_godot_string(mission_.level_id)); }

void CampaignMapNodeView::on_focus_entered() {
    focused_ = true;
    update_style();
    emit_signal("selected", to_godot_string(mission_.level_id));
}

void CampaignMapNodeView::on_focus_exited() {
    focused_ = false;
    update_style();
}

void CampaignMapNodeView::on_pressed() {
    emit_signal("selected", to_godot_string(mission_.level_id));
    if (mission_.state != CampaignNodeState::LOCKED) {
        emit_signal("activated", to_godot_string(mission_.level_id));
    }
}

void CampaignMapNodeView::update_style() {
    const GColor color = state_color(mission_.state);
    frame_->add_theme_stylebox_override("panel", frame_style(color, selected_));
    selection_ring_->add_theme_stylebox_override("panel", outline_style(color, 2));
    selection_ring_->set_visible(selected_);
    focus_ring_->add_theme_stylebox_override("panel", outline_style(GColor("f7e5a0"), 1));
    focus_ring_->set_visible(focused_);
    medallion_->set_text(state_icon(mission_.state));
    medallion_->add_theme_color_override("font_color", color);
    medallion_->add_theme_stylebox_override("normal", medallion_style(color));
    preview_->set_modulate(mission_.state == CampaignNodeState::LOCKED ? GColor(0.55F, 0.6F, 0.65F, 0.45F) : GColor(1, 1, 1, 1));
}

} // namespace defn
