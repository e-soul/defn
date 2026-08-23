// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "operation_dossier_view.h"

#include "campaign_preview_view.h"
#include "godot_string.h"
#include "ui_sfx_player.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"

#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>

namespace defn {

using namespace godot;
using GColor = godot::Color;

namespace {

Label *make_styled_label(std::string_view text_style) { return make_label({}, text_style); }

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

std::string_view status_color_role(CampaignNodeState state) {
    if (state == CampaignNodeState::COMPLETED) {
        return "state_success";
    }
    if (state == CampaignNodeState::LOCKED) {
        return "text_muted";
    }
    return "accent";
}

String upgraded_value(int base, int effective) {
    const int delta = effective - base;
    return delta == 0 ? String::num_int64(effective) : vformat("%d (+%d)", effective, delta);
}

} // namespace

OperationDossierView::OperationDossierView() {
    UiThemeProvider::apply_to(this);
    set_custom_minimum_size({UiThemeProvider::metric("operation_dossier_width", 464), UiThemeProvider::metric("operation_dossier_height", 866)});
    set_theme_type_variation(UiThemeProvider::panel_variation("dossier"));

    auto *content = memnew(VBoxContainer);
    content->set_name("DossierContent");
    content->add_theme_constant_override("separation", UiThemeProvider::spacing("md"));
    add_child(content);

    auto *top = memnew(HBoxContainer);
    eyebrow_ = make_styled_label("eyebrow");
    eyebrow_->set_name("OperationNumber");
    eyebrow_->set_h_size_flags(SIZE_EXPAND_FILL);
    top->add_child(eyebrow_);
    status_ = make_styled_label("eyebrow");
    status_->set_name("OperationStatus");
    status_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    top->add_child(status_);
    content->add_child(top);

    title_ = make_styled_label("dossier_title");
    title_->set_name("OperationTitle");
    title_->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    title_->set_custom_minimum_size({0.0F, UiThemeProvider::metric("operation_title_height", 78)});
    content->add_child(title_);

    tagline_ = make_styled_label("tagline");
    tagline_->set_name("OperationTagline");
    tagline_->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    tagline_->set_custom_minimum_size({0.0F, UiThemeProvider::metric("operation_text_block_height", 54)});
    content->add_child(tagline_);

    preview_ = memnew(CampaignPreviewView);
    preview_->set_name("OperationPreview");
    preview_->set_custom_minimum_size({UiThemeProvider::metric("operation_preview_width", 416), UiThemeProvider::metric("operation_preview_height", 234)});
    content->add_child(preview_);

    auto *intel_row = memnew(HBoxContainer);
    intel_row->set_name("IntelRow");
    intel_row->add_theme_constant_override("separation", UiThemeProvider::spacing("sm"));
    intel_row->add_child(make_stat_cell("THREAT", threat_value_));
    intel_row->add_child(make_stat_cell("DURATION", duration_value_));
    intel_row->add_child(make_stat_cell("WAVES", waves_value_));
    content->add_child(intel_row);

    enemy_heading_ = make_label("ENEMY PRESENCE", "eyebrow");
    enemy_heading_->set_name("EnemyHeading");
    content->add_child(enemy_heading_);

    enemy_chips_ = memnew(HFlowContainer);
    enemy_chips_->set_name("EnemyChips");
    enemy_chips_->add_theme_constant_override("separation", UiThemeProvider::spacing("sm"));
    content->add_child(enemy_chips_);

    auto *conditions_row = memnew(HBoxContainer);
    conditions_row->set_name("ConditionsRow");
    conditions_row->add_theme_constant_override("separation", UiThemeProvider::spacing("sm"));
    conditions_row->add_child(make_stat_cell("STARTING ENERGY", energy_value_));
    conditions_row->add_child(make_stat_cell("BASE INTEGRITY", integrity_value_));
    content->add_child(conditions_row);

    record_ = make_styled_label("body");
    record_->set_name("MissionRecord");
    record_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    content->add_child(record_);

    locked_message_ = make_styled_label("body");
    locked_message_->set_name("LockedMessage");
    set_state_tint(locked_message_, "state_danger");
    locked_message_->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    locked_message_->set_custom_minimum_size({0.0F, UiThemeProvider::metric("operation_text_block_height", 54)});
    content->add_child(locked_message_);

    auto *spacer = memnew(Control);
    spacer->set_v_size_flags(SIZE_EXPAND_FILL);
    spacer->set_mouse_filter(MOUSE_FILTER_IGNORE);
    content->add_child(spacer);

    deploy_button_ = make_button({}, "primary", callable_mp(this, &OperationDossierView::on_deploy_pressed));
    deploy_button_->set_name("PrimaryAction");
    deploy_button_->set_custom_minimum_size({0.0F, deploy_button_->get_custom_minimum_size().y});
    content->add_child(deploy_button_);

    back_button_ = make_button("BACK", "secondary", callable_mp(this, &OperationDossierView::on_back_pressed));
    back_button_->set_name("BackButton");
    back_button_->set_custom_minimum_size({0.0F, back_button_->get_custom_minimum_size().y});
    content->add_child(back_button_);
}

void OperationDossierView::attach_sfx(UiSfxPlayer *ui_sfx_player) {
    connect_sfx(ui_sfx_player, deploy_button_);
    connect_sfx(ui_sfx_player, back_button_);
}

void OperationDossierView::_bind_methods() {
    ADD_SIGNAL(MethodInfo("deploy_requested", PropertyInfo(Variant::STRING, "level_id")));
    ADD_SIGNAL(MethodInfo("back_requested"));
}

void OperationDossierView::configure(const CampaignMissionViewModel &mission, const Ref<Texture2D> &preview_texture) {
    mission_ = mission;
    eyebrow_->set_text(vformat("OPERATION %02d", mission.sequence_number));
    status_->set_text(status_text(mission.state));
    set_state_tint(status_, status_color_role(mission.state));
    title_->set_text(to_godot_string(mission.name).to_upper());
    tagline_->set_text(to_godot_string(mission.tagline));
    preview_->configure(preview_texture, mission.preview.focus_x, mission.preview.focus_y, mission.preview.dossier_zoom);
    preview_->set_modulate(mission.state == CampaignNodeState::LOCKED ? UiThemeProvider::color("locked_tint") : GColor(1, 1, 1, 1));
    threat_value_->set_text(to_godot_string(mission.threat_label).to_upper());
    duration_value_->set_text(to_godot_string(mission.duration_label));
    waves_value_->set_text(String::num_int64(mission.wave_count));
    energy_value_->set_text(upgraded_value(mission.base_starting_energy, mission.effective_starting_energy));
    integrity_value_->set_text(upgraded_value(mission.base_integrity, mission.effective_base_integrity));
    record_->set_text(mission.best_score > 0 ? vformat("BEST SCORE  %d", mission.best_score) : String("BEST SCORE  UNPLAYED"));

    clear_enemy_chips();
    if (mission.state != CampaignNodeState::LOCKED) {
        const auto chip_limit = static_cast<std::size_t>(UiThemeProvider::data().metric("operation_enemy_chip_limit", 4));
        const std::size_t visible_count = std::min<std::size_t>(chip_limit, mission.enemy_labels.size());
        for (std::size_t index = 0; index < visible_count; ++index) {
            enemy_chips_->add_child(make_chip(to_godot_string(mission.enemy_labels[index]), "text_primary"));
        }
        if (mission.enemy_labels.size() > visible_count) {
            enemy_chips_->add_child(make_chip(vformat("+%d", mission.enemy_labels.size() - visible_count), "text_muted"));
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
