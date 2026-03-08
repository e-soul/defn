#ifndef HUD_H
#define HUD_H

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <vector>

namespace defn {

using namespace godot;

class HUD : public CanvasLayer {
    GDCLASS(HUD, CanvasLayer)

public:
    HUD();

    void _ready() override;

    void update_aether(int value);
    void update_wave(int current, int total);
    void update_hearts(int integrity);
    void update_deploy_button(bool can_afford);
    void show_victory();
    void show_defeat();

protected:
    static void _bind_methods();

private:
    void build_ui();

    Label *aether_label = nullptr;
    Label *wave_label = nullptr;
    HBoxContainer *hearts_container = nullptr;
    std::vector<Label *> heart_icons;
    Label *deploy_label = nullptr;
    Label *end_game_label = nullptr;
};

} // namespace defn

#endif
