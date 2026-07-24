#include "campaign_preview_view.h"

#include "campaign_map_view_model.h"

#include <godot_cpp/classes/canvas_item.hpp>

namespace defn {

using namespace godot;

CampaignPreviewView::CampaignPreviewView() {
    set_clip_contents(true);
    set_mouse_filter(MOUSE_FILTER_IGNORE);
    texture_rect_ = memnew(TextureRect);
    texture_rect_->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
    texture_rect_->set_stretch_mode(TextureRect::STRETCH_SCALE);
    texture_rect_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    texture_rect_->set_texture_filter(CanvasItem::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS);
    add_child(texture_rect_);
}

void CampaignPreviewView::_bind_methods() {}

void CampaignPreviewView::_notification(int what) {
    if (what == NOTIFICATION_RESIZED) {
        update_framing();
    }
}

void CampaignPreviewView::configure(const Ref<Texture2D> &texture, float focus_x, float focus_y, float zoom) {
    focus_x_ = focus_x;
    focus_y_ = focus_y;
    zoom_ = zoom;
    texture_rect_->set_texture(texture);
    update_framing();
}

void CampaignPreviewView::update_framing() {
    const Ref<Texture2D> texture = texture_rect_->get_texture();
    if (!texture.is_valid()) {
        return;
    }
    const CampaignPreviewFrame frame = CampaignMapPresenter::frame_preview(static_cast<float>(texture->get_width()), static_cast<float>(texture->get_height()),
                                                                           get_size().x, get_size().y, focus_x_, focus_y_, zoom_);
    texture_rect_->set_position({frame.origin_x, frame.origin_y});
    texture_rect_->set_size({frame.draw_width, frame.draw_height});
}

} // namespace defn
