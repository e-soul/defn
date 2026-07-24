#ifndef CAMPAIGN_MAP_NODE_VIEW_H
#define CAMPAIGN_MAP_NODE_VIEW_H

#include "campaign_map_view_model.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

class CampaignPreviewView;

class CampaignMapNodeView : public godot::Control {
    GDCLASS(CampaignMapNodeView, godot::Control)

  public:
    CampaignMapNodeView();
    void configure(const CampaignMissionViewModel &mission, const godot::Ref<godot::Texture2D> &preview_texture);
    void set_selected(bool selected);
    void grab_node_focus();
    [[nodiscard]] const std::string &level_id() const { return mission_.level_id; }
    [[nodiscard]] godot::Button *button() const { return interaction_; }

  protected:
    static void _bind_methods();

  private:
    void on_pointer_entered();
    void on_focus_entered();
    void on_focus_exited();
    void on_pressed();
    void update_style();

    CampaignMissionViewModel mission_;
    bool selected_ = false;
    bool focused_ = false;
    godot::Panel *selection_ring_ = nullptr;
    godot::Panel *focus_ring_ = nullptr;
    godot::Panel *frame_ = nullptr;
    CampaignPreviewView *preview_ = nullptr;
    godot::Label *medallion_ = nullptr;
    godot::Button *interaction_ = nullptr;
};

} // namespace defn

#endif
