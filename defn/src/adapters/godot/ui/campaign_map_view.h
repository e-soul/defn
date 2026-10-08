// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef CAMPAIGN_MAP_VIEW_H
#define CAMPAIGN_MAP_VIEW_H

#include "campaign_map_view_model.h"
#include "ui_widgets.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/cpu_particles2d.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_flow_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace defn {

class CampaignMapNodeView;
class OperationDossierView;
class ProgressionService;
class CampaignPreviewView;
class MobileCampaignView;

class CampaignMapView : public UiContextControl {
    GDCLASS(CampaignMapView, UiContextControl)

  public:
    enum class LoadingState { WaitingToStart, LoadingTextures, Ready, Failed };

    CampaignMapView();
    void _process(double delta) override;
    void configure(ProgressionService *progression, const godot::Callable &deploy_action, const godot::Callable &back_action);
    void configure(CampaignMapViewModel view_model, const godot::Callable &deploy_action, const godot::Callable &back_action);
    /// Called when endless mode is deployed. Set alongside `deploy_action`; the map itself decides nothing about
    /// whether endless is available -- the view model already answered that by carrying an endless entry or not.
    void set_endless_action(const godot::Callable &endless_action) { endless_action_ = endless_action; }
    void navigate_back() { request_back(); }
    [[nodiscard]] LoadingState loading_state() const { return loading_state_; }
    [[nodiscard]] const std::string &selected_level_id() const { return selection_.level_id; }
    [[nodiscard]] const CampaignSelection &selected_item() const { return selection_; }
    [[nodiscard]] OperationDossierView *dossier() const { return dossier_; }

  protected:
    static void _bind_methods();
    void context_changed() override { layout_reference_surface(); }
    void _notification(int what);

  private:
    void configure_loading(const godot::Callable &deploy_action, const godot::Callable &back_action);
    void build_loading_overlay();
    void begin_loading();
    [[nodiscard]] bool compose_view_model();
    [[nodiscard]] bool queue_texture_requests();
    void poll_texture_requests();
    void complete_loading();
    void fail_loading(const godot::String &message);
    void retry_loading();
    void finish_overlay_fade();
    void update_loading_animation(double delta);
    [[nodiscard]] godot::Ref<godot::Texture2D> texture_for(const CampaignTextureDefinition &definition) const;
    void build_map_content();
    void build_mobile_content();
    void build_routes(godot::Control *route_layer);
    void build_nodes(godot::Control *node_layer);
    void build_endless_button(godot::HBoxContainer *header_row);
    void select_level(const godot::String &level_id);
    void select_item(const CampaignSelection &selection);
    void activate_level(const godot::String &level_id);
    void deploy_selected();
    void deploy_endless();
    void fade_in_dossier();
    void request_back();
    void layout_reference_surface();
    void configure_ambience(const CampaignMissionViewModel &mission);
    [[nodiscard]] const CampaignMissionViewModel *find_mission(const std::string &level_id) const;

    CampaignMapViewModel view_model_;
    std::optional<CampaignMapViewModel> supplied_view_model_;
    ProgressionService *progression_ = nullptr;
    CampaignSelection selection_;
    godot::Callable deploy_action_;
    godot::Callable back_action_;
    LoadingState loading_state_ = LoadingState::WaitingToStart;
    godot::Control *loading_overlay_ = nullptr;
    godot::Label *loading_spinner_ = nullptr;
    godot::Label *loading_status_ = nullptr;
    godot::HFlowContainer *loading_actions_ = nullptr;
    double loading_animation_elapsed_ = 0.0;
    std::vector<std::string> requested_texture_paths_;
    std::unordered_map<std::string, godot::Ref<godot::Texture2D>> loaded_textures_;
    godot::Control *reference_surface_ = nullptr;
    godot::Control *wide_ui_ = nullptr;
    MobileCampaignView *mobile_root_ = nullptr;
    OperationDossierView *dossier_ = nullptr;
    godot::Callable endless_action_;
    godot::CPUParticles2D *ambience_ = nullptr;
    std::vector<CampaignMapNodeView *> node_views_;
};

} // namespace defn

#endif
