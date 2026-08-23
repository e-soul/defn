// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UI_SFX_PLAYER_H
#define UI_SFX_PLAYER_H

#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <string_view>

namespace defn {

struct UiSfxData;
struct UiSoundData;

class UiSfxPlayer : public godot::Node {
    GDCLASS(UiSfxPlayer, godot::Node)

  public:
    void configure(const UiSfxData &config);
    /// Hover plus the press sound the variant named. One entry point, so a control cannot end up silent because
    /// its call site forgot which of the two wiring calls applied to it.
    void connect_button(godot::BaseButton *button, std::string_view press_role);

    /// The player the UI wires new controls to. Set when one is configured; narrow to this one purpose.
    static UiSfxPlayer *active();

  protected:
    static void _bind_methods();

  private:
    godot::AudioStreamPlayer *create_player(const char *name, const UiSoundData &sound);
    void play_hover(godot::BaseButton *button);
    void play_press(godot::BaseButton *button, bool deploy_card);

    godot::AudioStreamPlayer *hover_player_ = nullptr;
    godot::AudioStreamPlayer *click_player_ = nullptr;
    godot::AudioStreamPlayer *deploy_card_player_ = nullptr;
};

} // namespace defn

#endif
