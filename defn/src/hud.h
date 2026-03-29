#ifndef HUD_H
#define HUD_H

#include "unit_data.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <vector>

namespace defn {

using namespace godot;

struct DeployCardUI {
    String unit_type;
    int cost = 0;
    Button *button = nullptr;
};

class HUD : public CanvasLayer {
    GDCLASS(HUD, CanvasLayer)

  public:
    HUD();

    void _ready() override;

    void set_friendly_units(const std::vector<UnitConfig> &units);
    void update_core_resource(int value);
    void update_wave(int current, int total);
    void update_hearts(int integrity);
    void update_card_affordability(int energy);
    void update_score(int score);
    void show_score_screen(const Dictionary &stats);

  protected:
    static void _bind_methods();

  private:
    void build_ui();
    void on_card_pressed(const String &unit_type);
    void on_next_level_pressed(const String &level_id);
    void on_retry_pressed(const String &level_id);
    void on_main_menu_pressed();

    Label *core_resource_label = nullptr;
    Label *wave_label = nullptr;
    Label *score_label = nullptr;
    HBoxContainer *hearts_container = nullptr;
    std::vector<Label *> heart_icons;
    HBoxContainer *card_container = nullptr;
    std::vector<DeployCardUI> deploy_cards;

    // Score screen
    ColorRect *score_screen_overlay = nullptr;
    PanelContainer *score_screen_panel = nullptr;
};

} // namespace defn

#endif
