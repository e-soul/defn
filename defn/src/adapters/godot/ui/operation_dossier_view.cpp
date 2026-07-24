#include "operation_dossier_view.h"

#include "campaign_preview_view.h"
#include "godot_string.h"

#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>

namespace defn {

using namespace godot;
using GColor = godot::Color;

namespace {

Ref<StyleBoxFlat> dossier_style() {
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(GColor(0.035F, 0.067F, 0.09F, 0.96F));
    style->set_border_color(GColor("58656a"));
    style->set_border_width_all(2);
    style->set_corner_radius_all(8);
    style->set_content_margin_all(22.0F);
    style->set_shadow_color(GColor(0, 0, 0, 0.72F));
    style->set_shadow_size(22);
    return style;
}

Label *make_label(int font_size, const GColor &color) {
    auto *label = memnew(Label);
    label->add_theme_font_size_override("font_size", font_size);
    label->add_theme_color_override("font_color", color);
    label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    return label;
}

VBoxContainer *make_stat_cell(const String &heading, Label *&value) {
    auto *cell = memnew(VBoxContainer);
    cell->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    cell->add_theme_constant_override("separation", 2);
    auto *heading_label = make_label(13, GColor("9eadae"));
    heading_label->set_text(heading);
    heading_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    cell->add_child(heading_label);
    value = make_label(18, GColor("e8ddc3"));
    value->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    cell->add_child(value);
    return cell;
}

Ref<StyleBoxFlat> chip_style() {
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(GColor(0.10F, 0.16F, 0.20F, 0.94F));
    style->set_border_color(GColor("58656a"));
    style->set_border_width_all(1);
    style->set_corner_radius_all(4);
    style->set_content_margin(SIDE_LEFT, 9.0F);
    style->set_content_margin(SIDE_RIGHT, 9.0F);
    style->set_content_margin(SIDE_TOP, 4.0F);
    style->set_content_margin(SIDE_BOTTOM, 4.0F);
    return style;
}

PanelContainer *make_enemy_chip(const String &text, const GColor &color) {
    auto *chip = memnew(PanelContainer);
    chip->add_theme_stylebox_override("panel", chip_style());
    auto *label = make_label(15, color);
    label->set_text(text);
    chip->add_child(label);
    return chip;
}

String status_text(CampaignNodeState state) {
    switch (state) {
    case CampaignNodeState::COMPLETED:
        return "SECURED";
    case CampaignNodeState::AVAILABLE:
    case CampaignNodeState::FRONTIER:
        return "AVAILABLE";
    case CampaignNodeState::LOCKED:
        return "LOCKED";
    }
    return "LOCKED";
}

GColor status_color(CampaignNodeState state) {
    if (state == CampaignNodeState::COMPLETED) {
        return {"5fcb9a"};
    }
    if (state == CampaignNodeState::LOCKED) {
        return {"9eadae"};
    }
    return {"f2be55"};
}

String upgraded_value(int base, int effective) {
    const int delta = effective - base;
    return delta == 0 ? String::num_int64(effective) : vformat("%d (+%d)", effective, delta);
}

void theme_action_button(Button *button, bool primary) {
    button->set_custom_minimum_size({0.0F, 54.0F});
    button->add_theme_font_size_override("font_size", 22);
    button->add_theme_color_override("font_color", primary ? GColor("0a1118") : GColor("e8ddc3"));
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(primary ? GColor("f2be55") : GColor("18252f"));
    style->set_border_color(primary ? GColor("f7e5a0") : GColor("58656a"));
    style->set_border_width_all(2);
    style->set_corner_radius_all(4);
    button->add_theme_stylebox_override("normal", style);
}

} // namespace

OperationDossierView::OperationDossierView() {
    set_custom_minimum_size({464.0F, 866.0F});
    add_theme_stylebox_override("panel", dossier_style());

    auto *content = memnew(VBoxContainer);
    content->set_name("DossierContent");
    content->add_theme_constant_override("separation", 11);
    add_child(content);

    auto *top = memnew(HBoxContainer);
    eyebrow_ = make_label(18, GColor("9eadae"));
    eyebrow_->set_name("OperationNumber");
    eyebrow_->set_h_size_flags(SIZE_EXPAND_FILL);
    top->add_child(eyebrow_);
    status_ = make_label(18, GColor("f2be55"));
    status_->set_name("OperationStatus");
    status_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    top->add_child(status_);
    content->add_child(top);

    title_ = make_label(34, GColor("e8ddc3"));
    title_->set_name("OperationTitle");
    title_->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    title_->set_custom_minimum_size({0.0F, 78.0F});
    content->add_child(title_);

    tagline_ = make_label(18, GColor("b9c4c3"));
    tagline_->set_name("OperationTagline");
    tagline_->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    tagline_->set_custom_minimum_size({0.0F, 54.0F});
    content->add_child(tagline_);

    preview_ = memnew(CampaignPreviewView);
    preview_->set_name("OperationPreview");
    preview_->set_custom_minimum_size({416.0F, 234.0F});
    content->add_child(preview_);

    auto *intel_row = memnew(HBoxContainer);
    intel_row->set_name("IntelRow");
    intel_row->add_theme_constant_override("separation", 8);
    intel_row->add_child(make_stat_cell("THREAT", threat_value_));
    intel_row->add_child(make_stat_cell("DURATION", duration_value_));
    intel_row->add_child(make_stat_cell("WAVES", waves_value_));
    content->add_child(intel_row);

    enemy_heading_ = make_label(16, GColor("9eadae"));
    enemy_heading_->set_name("EnemyHeading");
    enemy_heading_->set_text("ENEMY PRESENCE");
    content->add_child(enemy_heading_);

    enemy_chips_ = memnew(HFlowContainer);
    enemy_chips_->set_name("EnemyChips");
    enemy_chips_->add_theme_constant_override("separation", 7);
    content->add_child(enemy_chips_);

    auto *conditions_row = memnew(HBoxContainer);
    conditions_row->set_name("ConditionsRow");
    conditions_row->add_theme_constant_override("separation", 8);
    conditions_row->add_child(make_stat_cell("STARTING ENERGY", energy_value_));
    conditions_row->add_child(make_stat_cell("BASE INTEGRITY", integrity_value_));
    content->add_child(conditions_row);

    record_ = make_label(18, GColor("e8ddc3"));
    record_->set_name("MissionRecord");
    record_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    content->add_child(record_);

    locked_message_ = make_label(18, GColor("d7a39b"));
    locked_message_->set_name("LockedMessage");
    locked_message_->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    locked_message_->set_custom_minimum_size({0.0F, 54.0F});
    content->add_child(locked_message_);

    auto *spacer = memnew(Control);
    spacer->set_v_size_flags(SIZE_EXPAND_FILL);
    spacer->set_mouse_filter(MOUSE_FILTER_IGNORE);
    content->add_child(spacer);

    deploy_button_ = memnew(Button);
    deploy_button_->set_name("PrimaryAction");
    theme_action_button(deploy_button_, true);
    deploy_button_->connect("pressed", callable_mp(this, &OperationDossierView::on_deploy_pressed));
    content->add_child(deploy_button_);

    back_button_ = memnew(Button);
    back_button_->set_name("BackButton");
    back_button_->set_text("BACK");
    theme_action_button(back_button_, false);
    back_button_->connect("pressed", callable_mp(this, &OperationDossierView::on_back_pressed));
    content->add_child(back_button_);
}

void OperationDossierView::_bind_methods() {
    ADD_SIGNAL(MethodInfo("deploy_requested", PropertyInfo(Variant::STRING, "level_id")));
    ADD_SIGNAL(MethodInfo("back_requested"));
}

void OperationDossierView::configure(const CampaignMissionViewModel &mission, const Ref<Texture2D> &preview_texture) {
    mission_ = mission;
    eyebrow_->set_text(vformat("OPERATION %02d", mission.sequence_number));
    status_->set_text(status_text(mission.state));
    status_->add_theme_color_override("font_color", status_color(mission.state));
    title_->set_text(to_godot_string(mission.name).to_upper());
    tagline_->set_text(to_godot_string(mission.tagline));
    preview_->configure(preview_texture, mission.preview.focus_x, mission.preview.focus_y, mission.preview.dossier_zoom);
    preview_->set_modulate(mission.state == CampaignNodeState::LOCKED ? GColor(0.5F, 0.55F, 0.62F, 0.5F) : GColor(1, 1, 1, 1));
    threat_value_->set_text(to_godot_string(mission.threat_label).to_upper());
    duration_value_->set_text(to_godot_string(mission.duration_label));
    waves_value_->set_text(String::num_int64(mission.wave_count));
    energy_value_->set_text(upgraded_value(mission.base_starting_energy, mission.effective_starting_energy));
    integrity_value_->set_text(upgraded_value(mission.base_integrity, mission.effective_base_integrity));
    record_->set_text(mission.best_score > 0 ? vformat("BEST SCORE  %d", mission.best_score) : String("BEST SCORE  UNPLAYED"));

    clear_enemy_chips();
    if (mission.state != CampaignNodeState::LOCKED) {
        const std::size_t visible_count = std::min<std::size_t>(4, mission.enemy_labels.size());
        for (std::size_t index = 0; index < visible_count; ++index) {
            enemy_chips_->add_child(make_enemy_chip(to_godot_string(mission.enemy_labels[index]), GColor("d4d9d6")));
        }
        if (mission.enemy_labels.size() > visible_count) {
            enemy_chips_->add_child(make_enemy_chip(vformat("+%d", mission.enemy_labels.size() - visible_count), GColor("9eadae")));
        }
    }

    const bool locked = mission.state == CampaignNodeState::LOCKED;
    enemy_heading_->set_visible(!locked);
    enemy_chips_->set_visible(!locked);
    locked_message_->set_visible(locked);
    locked_message_->set_text(locked ? "ROUTE BLOCKED\n" + to_godot_string(mission.unlock_requirement) : String());
    deploy_button_->set_disabled(locked);
    String action_label = "DEPLOY";
    if (locked) {
        action_label = "LOCKED";
    } else if (mission.state == CampaignNodeState::COMPLETED) {
        action_label = "REPLAY";
    }
    deploy_button_->set_text(action_label);
    deploy_button_->set_tooltip_text(locked ? to_godot_string(mission.unlock_requirement) : String());
}

void OperationDossierView::on_deploy_pressed() {
    if (mission_.state != CampaignNodeState::LOCKED) {
        emit_signal("deploy_requested", to_godot_string(mission_.level_id));
    }
}

void OperationDossierView::on_back_pressed() { emit_signal("back_requested"); }

void OperationDossierView::clear_enemy_chips() {
    while (enemy_chips_->get_child_count() > 0) {
        Node *child = enemy_chips_->get_child(0);
        enemy_chips_->remove_child(child);
        child->queue_free();
    }
}

} // namespace defn
