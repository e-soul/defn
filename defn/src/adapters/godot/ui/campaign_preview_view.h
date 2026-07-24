#ifndef CAMPAIGN_PREVIEW_VIEW_H
#define CAMPAIGN_PREVIEW_VIEW_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

class CampaignPreviewView : public godot::Control {
    GDCLASS(CampaignPreviewView, godot::Control)

  public:
    CampaignPreviewView();
    void configure(const godot::Ref<godot::Texture2D> &texture, float focus_x, float focus_y, float zoom);

  protected:
    static void _bind_methods();
    void _notification(int what);

  private:
    void update_framing();

    godot::TextureRect *texture_rect_ = nullptr;
    float focus_x_ = 0.5F;
    float focus_y_ = 0.5F;
    float zoom_ = 1.0F;
};

} // namespace defn

#endif
