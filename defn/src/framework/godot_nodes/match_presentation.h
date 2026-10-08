// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef MATCH_PRESENTATION_H
#define MATCH_PRESENTATION_H
#include "responsive_ui_root.h"
#include "unit_data.h"
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/sub_viewport_container.hpp>
namespace defn {
class HUD;
class UnitSelectionController;
class MatchPresentation : public ResponsiveUiRoot {
    GDCLASS(MatchPresentation, ResponsiveUiRoot)
  public:
    void _ready() override;
    godot::Node *ui_parent() const { return ui_; }
    void configure_hud(HUD *hud, const UnitDataLoader &content, godot::Node2D *base, UnitSelectionController *selection);

  protected:
    static void _bind_methods() {}
    void apply_layout() override;

  private:
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
