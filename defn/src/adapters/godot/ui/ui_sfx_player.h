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
    /// Mounts the one player the UI wires its controls to, under `owner`, and hands back the player already
    /// serving this tree when there is one. Screens install rather than construct, so a scene never grows a
    /// second player that half its controls would end up wired to instead.
    static UiSfxPlayer *install(godot::Node *owner);
    /// Hover plus the press sound the variant named. One entry point, so a control cannot end up silent because
    /// its call site forgot which of the two wiring calls applied to it.
    void connect_button(godot::BaseButton *button, std::string_view press_role);

    /// The player the UI wires new controls to, and the only route to one: a control is wired by whatever
    /// builds it, through this, so no second caller can arrive later and wire it again.
    static UiSfxPlayer *active();

  protected:
    static void _bind_methods();

  private:
    void configure(const UiSfxData &config);
    godot::AudioStreamPlayer *create_player(const char *name, const UiSoundData &sound);
    void play_hover(godot::BaseButton *button);
    void play_press(godot::BaseButton *button, bool deploy_card);

    godot::AudioStreamPlayer *hover_player_ = nullptr;
    godot::AudioStreamPlayer *click_player_ = nullptr;
    godot::AudioStreamPlayer *deploy_card_player_ = nullptr;
};

} // namespace defn

#endif
