// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef HUD_METERS_H
#define HUD_METERS_H

#include "hud_presenter.h"
#include "ui_theme_models.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/color.hpp>

namespace defn {

/// One skewed segment per point of starting integrity, with the leading segment draining continuously so damage
/// smaller than a whole segment still registers.
class HudIntegrityMeter : public godot::Control {
    GDCLASS(HudIntegrityMeter, godot::Control)

  public:
    HudIntegrityMeter();

    void configure(const HudIntegrityModel &model, const godot::Color &color);
    void apply_appearance(const UiThemeData &theme);
    void _draw() override;

    [[nodiscard]] int get_segment_count() const;

  protected:
    static void _bind_methods();

  private:
    HudIntegrityModel model_;
    godot::Color color_;
    float segment_width_ = 26;
    float segment_height_ = 15;
    float segment_gap_ = 4;
    float outline_width_ = 1;
    float max_width_ = 160;
    godot::Color track_color_;
    godot::Color outline_color_;
};

} // namespace defn

#endif
