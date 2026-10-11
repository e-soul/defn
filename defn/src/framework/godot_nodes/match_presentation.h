// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef MATCH_PRESENTATION_H
#define MATCH_PRESENTATION_H
#include "match_resources.h"
#include "responsive_ui_root.h"
#include "unit_data.h"
#include <godot_cpp/classes/h_flow_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/sub_viewport_container.hpp>
#include <memory>
namespace defn {
class HUD;
class UnitSelectionController;
class GameManager;
class CampaignService;
class MatchPresentation : public ResponsiveUiRoot {
    GDCLASS(MatchPresentation, ResponsiveUiRoot)
  public:
    void _ready() override;
    void _process(double delta) override;
    [[nodiscard]] bool is_match_ready() const { return loading_state_ == LoadingState::Ready; }
    [[nodiscard]] bool has_loading_failed() const { return loading_state_ == LoadingState::Failed; }
    godot::Node *ui_parent() const { return ui_; }
    void configure_hud(HUD *hud, const UnitDataLoader &content, godot::Node2D *base, UnitSelectionController *selection);

  protected:
    static void _bind_methods();
    void apply_layout() override;

  private:
    enum class LoadingState { ShowingOverlay, Preparing, LoadingResources, BuildingAnimations, BuildingMatch, Rendering, Ready, Failed };
    void build_loading_overlay();
    void prepare_content();
    bool select_units(PreparedMatch &match, CampaignService &progression);
    void load_resources();
    void load_threaded_resources();
    void load_incremental_resources();
    void build_battlefield();
    void render_resources();
    void frame_drawn();
    void fail_loading(const godot::String &message);
    void fail_resource(const godot::String &path);
    void retry_loading();
    void return_to_menu();
    LoadingState loading_state_ = LoadingState::ShowingOverlay;
    godot::Control *loading_overlay_ = nullptr;
    godot::Label *loading_status_ = nullptr;
    godot::Label *loading_spinner_ = nullptr;
    godot::HFlowContainer *loading_actions_ = nullptr;
    std::optional<PreparedMatch> prepared_;
    std::shared_ptr<MatchResources> resources_;
    std::vector<UnitConfig> selected_units_;
    std::vector<std::string> resource_paths_;
    std::vector<std::string> pending_paths_;
    std::size_t requested_ = 0;
    std::size_t loaded_ = 0;
    std::size_t animation_index_ = 0;
    std::size_t render_index_ = 0;
    bool frame_drawn_ = false;
    int loading_frames_ = 0;
    double spinner_elapsed_ = 0;
    bool threaded_loading_ = true;
    GameManager *game_ = nullptr;
    godot::Node2D *warmup_ = nullptr;
    godot::SubViewportContainer *host_ = nullptr;
    godot::SubViewport *world_ = nullptr;
    godot::Control *ui_ = nullptr;
    HUD *hud_ = nullptr;
    UnitSelectionController *selection_ = nullptr;
    UiSize world_size_;
    float protected_top_ = 0;
    float protected_bottom_ = 1;
    float sky_left_ = 0;
};
} // namespace defn
#endif
