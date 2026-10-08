// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef DEPLOY_TRAY_H
#define DEPLOY_TRAY_H
#include "deploy_card_presenter.h"
#include "deploy_card_view_model.h"
#include "responsive_layout.h"
#include "ui_theme_provider.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <vector>
namespace defn {
class DeployTray : public godot::Control {
    GDCLASS(DeployTray, godot::Control)
  public:
    void _ready() override;
    void _gui_input(const godot::Ref<godot::InputEvent> &event) override;
    void _input(const godot::Ref<godot::InputEvent> &event) override;
    void _notification(int what);
    godot::Button *add_card(const DeployCardViewModel &model);
    void clear_cards();
    void apply_layout(const MatchLayout &layout);
    void apply_appearance(const UiAppearance &appearance);

  protected:
    static void _bind_methods() {}

  private:
    void initialize_controls();
    void position_cards();
    void update_scroll();
    [[nodiscard]] const UiThemeData &card_theme() const;
    [[nodiscard]] UiCardGeometry card_geometry() const;
    void position_buttons(UiCardGeometry card, float visible_height, float gap, int columns);
    void style_card(const DeployCardNodes &nodes) const;
    void style_card_text(const DeployCardNodes &nodes, UiCardGeometry card, const UiThemeData &theme) const;
    void scroll_previous();
    void scroll_next();
    void focus_card(godot::Button *card);
    void cancel_gesture();
    void handle_gesture(const godot::Ref<godot::InputEvent> &event, godot::Vector2 point);
    godot::Control *clip_ = nullptr;
    godot::Control *cards_ = nullptr;
    godot::Button *previous_ = nullptr;
    godot::Button *next_ = nullptr;
    std::vector<DeployCardNodes> card_nodes_;
    UiAppearance appearance_;
    TrayLayout geometry_;
    MatchLayout layout_;
    float scroll_ = 0;
    float max_scroll_ = 0;
    float start_scroll_ = 0;
    float card_hit_height_ = 0;
    godot::Vector2 start_;
    godot::Button *pressed_ = nullptr;
    bool gesture_ = false;
    bool dragged_ = false;
};
} // namespace defn
#endif
