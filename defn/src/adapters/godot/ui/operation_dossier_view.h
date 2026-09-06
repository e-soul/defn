// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef OPERATION_DOSSIER_VIEW_H
#define OPERATION_DOSSIER_VIEW_H

#include "campaign_map_view_model.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_flow_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <string>
#include <vector>

namespace defn {

class CampaignPreviewView;

class OperationDossierView : public godot::PanelContainer {
    GDCLASS(OperationDossierView, godot::PanelContainer)

  public:
    OperationDossierView();
    void configure(const CampaignMissionViewModel &mission, const godot::Ref<godot::Texture2D> &preview_texture);

    /// The same panel and the same rows, with the bounded readings replaced by their unbounded forms. Endless is a
    /// standing engagement rather than an operation, so it says so where the operation number goes.
    void configure_endless(const CampaignEndlessViewModel &endless, const godot::Ref<godot::Texture2D> &preview_texture);
    [[nodiscard]] godot::Button *deploy_button() const { return deploy_button_; }
    [[nodiscard]] godot::Button *back_button() const { return back_button_; }

  protected:
    static void _bind_methods();

  private:
    void on_deploy_pressed();
    void set_enemy_chips(const std::vector<std::string> &labels);
    void on_back_pressed();
    void clear_enemy_chips();

    CampaignMissionViewModel mission_;
    bool endless_selected_ = false;
    godot::Label *eyebrow_ = nullptr;
    godot::Label *status_ = nullptr;
    godot::Label *title_ = nullptr;
    godot::Label *tagline_ = nullptr;
    CampaignPreviewView *preview_ = nullptr;
    godot::Label *threat_value_ = nullptr;
    godot::Label *duration_value_ = nullptr;
    godot::Label *waves_value_ = nullptr;
    godot::Label *enemy_heading_ = nullptr;
    godot::HFlowContainer *enemy_chips_ = nullptr;
    godot::Label *energy_value_ = nullptr;
    godot::Label *integrity_value_ = nullptr;
    godot::Label *record_ = nullptr;
    godot::Label *locked_message_ = nullptr;
    godot::Button *deploy_button_ = nullptr;
    godot::Button *back_button_ = nullptr;
};

} // namespace defn

#endif
