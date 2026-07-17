#ifndef UI_SFX_PLAYER_H
#define UI_SFX_PLAYER_H

#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

struct UiSfxData;
struct UiSoundData;

class UiSfxPlayer : public godot::Node {
    GDCLASS(UiSfxPlayer, godot::Node)

  public:
    void configure(const UiSfxData &config);
    void connect_menu_button(godot::BaseButton *button);
    void connect_deploy_card(godot::BaseButton *button);

  protected:
    static void _bind_methods();

  private:
    godot::AudioStreamPlayer *create_player(const char *name, const UiSoundData &sound);
    void play_hover(godot::BaseButton *button);
    void play_click(godot::BaseButton *button);
    void play_deploy_card(godot::BaseButton *button);

    godot::AudioStreamPlayer *hover_player_ = nullptr;
    godot::AudioStreamPlayer *click_player_ = nullptr;
    godot::AudioStreamPlayer *deploy_card_player_ = nullptr;
};

} // namespace defn

#endif
