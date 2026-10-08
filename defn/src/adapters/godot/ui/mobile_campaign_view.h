// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef MOBILE_CAMPAIGN_VIEW_H
#define MOBILE_CAMPAIGN_VIEW_H

#include "campaign_map_view_model.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <array>
#include <functional>
#include <vector>

namespace defn {
class CampaignPreviewView;

struct MobileCampaignActions {
    std::function<void(const CampaignSelection &)> select;
    godot::Callable deploy;
    godot::Callable endless;
    godot::Callable back;
};

/// Browses the existing campaign presentation; progression and deployment stay with its owner.
/// The selected item survives reflow. Pointer gestures operate only on the illustration/briefing.
class MobileCampaignView : public godot::Control {
    GDCLASS(MobileCampaignView, godot::Control)

  public:
    void configure(const CampaignMapViewModel &model, std::vector<godot::Ref<godot::Texture2D>> previews, const MobileCampaignActions &actions);
    void select_level(const godot::String &level_id);
    void select_item(const CampaignSelection &selection);
    [[nodiscard]] size_t selected_index() const { return selected_; }
    void _gui_input(const godot::Ref<godot::InputEvent> &event) override;
    void _input(const godot::Ref<godot::InputEvent> &event) override;

  protected:
    static void _bind_methods() {}
    void _notification(int what);

  private:
    void build_controls();
    void update_content();
    void layout();
    void layout_briefing(bool landscape, bool short_screen, float gap);
    void layout_badges(float gap, float header, int supporting);
    void layout_navigation(bool landscape, float gap, const godot::Rect2 &footer);
    void turn_page(int direction);
    void deploy();
    void cancel_gesture();
    [[nodiscard]] size_t item_count() const;
    [[nodiscard]] float text_height(const godot::String &text, float width, int size) const;
    static void place_text(godot::Label *label, const godot::Rect2 &rect, int size, std::string_view color = "text_primary");
    godot::Button *add_button(const godot::String &name, const godot::String &text, const godot::Callable &pressed, std::string_view variant = "secondary");

    CampaignMapViewModel model_;
    std::vector<godot::Ref<godot::Texture2D>> previews_;
    MobileCampaignActions actions_;
    size_t selected_ = 0;
    godot::Panel *card_ = nullptr;
    CampaignPreviewView *preview_ = nullptr;
    godot::Panel *operation_badge_ = nullptr;
    godot::Panel *status_badge_ = nullptr;
    godot::Panel *requirement_plate_ = nullptr;
    godot::Label *operation_ = nullptr;
    godot::Label *status_ = nullptr;
    godot::Label *requirement_ = nullptr;
    godot::Label *heading_ = nullptr;
    godot::Label *secured_ = nullptr;
    godot::Label *title_ = nullptr;
    godot::Label *tagline_ = nullptr;
    std::array<godot::Label *, 3> stat_names_{};
    std::array<godot::Label *, 3> stat_values_{};
    std::vector<godot::Panel *> indicators_;
    godot::Label *page_count_ = nullptr;
    godot::Button *previous_ = nullptr;
    godot::Button *next_ = nullptr;
    godot::Button *back_ = nullptr;
    godot::Button *deploy_ = nullptr;
    godot::Vector2 gesture_start_;
    bool gesture_ = false;
    bool laying_out_ = false;
};
} // namespace defn
#endif
