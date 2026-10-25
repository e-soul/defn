// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef DEPLOY_TUTORIAL_PROMPT_H
#define DEPLOY_TUTORIAL_PROMPT_H

#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/panel_container.hpp>

namespace defn {

/// A non-interactive cue; the HUD owns its target card and cancels the cue before rebuilding the roster.
class DeployTutorialPrompt : public godot::Control {
    GDCLASS(DeployTutorialPrompt, godot::Control)

  public:
    static constexpr const char *SOUND_PATH = "res://assets/sfx/tutorial_pop.wav";
    void _ready() override;
    void _process(double delta) override;
    void _draw() override;
    void start(godot::Button *target);
    void dismiss();
    void cancel();

  protected:
    static void _bind_methods() {}

  private:
    void position_above_card();
    godot::Button *target_ = nullptr;
    godot::PanelContainer *caption_ = nullptr;
    godot::AudioStreamPlayer *sound_ = nullptr;
    double elapsed_ = 0;
    double fade_elapsed_ = 0;
    float fade_alpha_ = 1;
    float arrow_x_ = 0;
    float caption_width_ = 0;
    bool dismissing_ = false;
};

} // namespace defn
#endif
