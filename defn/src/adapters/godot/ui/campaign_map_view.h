#ifndef CAMPAIGN_MAP_VIEW_H
#define CAMPAIGN_MAP_VIEW_H

#include "campaign_map_view_model.h"
#include "campaign_texture_cache.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/cpu_particles2d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable.hpp>

#include <string>
#include <vector>

namespace defn {

class CampaignMapNodeView;
class OperationDossierView;
class UiSfxPlayer;

class CampaignMapView : public godot::Control {
    GDCLASS(CampaignMapView, godot::Control)

  public:
    CampaignMapView();
    void _ready() override;
    void configure(CampaignMapViewModel view_model, const godot::Callable &deploy_action, const godot::Callable &back_action, UiSfxPlayer *ui_sfx_player);
    [[nodiscard]] const std::string &selected_level_id() const { return selected_level_id_; }
    [[nodiscard]] OperationDossierView *dossier() const { return dossier_; }
    void _unhandled_input(const godot::Ref<godot::InputEvent> &event) override;

  protected:
    static void _bind_methods();
    void _notification(int what);

  private:
    void build_screen(UiSfxPlayer *ui_sfx_player);
    void build_routes(godot::Control *route_layer);
    void build_nodes(godot::Control *node_layer, UiSfxPlayer *ui_sfx_player);
    void select_level(const godot::String &level_id);
    void activate_level(const godot::String &level_id);
    void deploy_selected();
    void request_back();
    void select_relative(int offset);
    void layout_reference_surface();
    void focus_selected_node();
    void configure_ambience(const CampaignMissionViewModel &mission);
    [[nodiscard]] const CampaignMissionViewModel *find_mission(const std::string &level_id) const;

    CampaignMapViewModel view_model_;
    std::string selected_level_id_;
    godot::Callable deploy_action_;
    godot::Callable back_action_;
    godot::Control *reference_surface_ = nullptr;
    OperationDossierView *dossier_ = nullptr;
    godot::CPUParticles2D *ambience_ = nullptr;
    godot::Ref<CampaignTextureCache> texture_cache_;
    std::vector<CampaignMapNodeView *> node_views_;
};

} // namespace defn

#endif
