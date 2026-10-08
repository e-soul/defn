// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef OPTIONS_SCREEN_VIEW_H
#define OPTIONS_SCREEN_VIEW_H

#include "ui_screen_scaffold.h"
#include <godot_cpp/classes/h_slider.hpp>
#include <vector>

namespace defn {

struct VolumeOptionControls {
    godot::HBoxContainer *row = nullptr;
    godot::Label *name = nullptr;
    godot::HSlider *slider = nullptr;
    godot::Label *value = nullptr;
};

/// Settings remain with SettingsRuntime; this view retains and reflows their controls.
class OptionsScreenView : public UiScreenControl {
    GDCLASS(OptionsScreenView, UiScreenControl)

  public:
    void add_desktop_control(godot::Control *control);
    void add_volume_control(const VolumeOptionControls &controls);

  protected:
    static void _bind_methods() {}
    void context_changed() override;

  private:
    struct VolumeRow {
        godot::VBoxContainer *column;
        VolumeOptionControls controls;
    };
    void reflow_options();
    static void reflow_volume(const VolumeRow &volume, bool audio_only);
    std::vector<godot::Control *> desktop_controls_;
    std::vector<VolumeRow> volume_rows_;
};

} // namespace defn
#endif
