#ifndef HUD_H
#define HUD_H

#include "hud_presenter.h"
#include "score_screen_models.h"
#include "unit_data.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <vector>

namespace defn {

using namespace godot;

struct DeployCardUI {
  std::string unit_type;
    Button *button = nullptr;
};

class HUD : public CanvasLayer {
    GDCLASS(HUD, CanvasLayer)

  public:
    HUD();

    void _ready() override;

    void set_friendly_units(const std::vector<UnitConfig> &units);
    void set_level(int level_number, const String &level_name);
    void update_core_resource(int value);
    void update_wave(int current, int total);
    void update_hearts(int integrity);
    void update_card_affordability(int energy);
    void update_score(int score);
    void show_score_screen(const ScoreScreenModel &summary);

  protected:
    static void _bind_methods();

  private:
    void build_ui();
    void render(const HudModel &model);
    void render_deploy_cards(const std::vector<HudDeployCardModel> &cards);
    void clear_deploy_cards();
    void ensure_heart_icons(int count);
    void on_card_pressed(const String &unit_type);
    void on_next_level_pressed(const String &level_id);
    void on_retry_pressed(const String &level_id);
    void on_main_menu_pressed();
    void on_upgrade_card_pressed(const String &upgrade_id);

    Label *core_resource_label = nullptr;
    Label *level_label = nullptr;
    Label *wave_label = nullptr;
    Label *score_label = nullptr;
    HBoxContainer *hearts_container = nullptr;
    std::vector<Label *> heart_icons;
    HBoxContainer *card_container = nullptr;
    std::vector<DeployCardUI> deploy_cards;
    HudPresentationInput hud_input_{.energy = 100, .current_wave = 1, .total_waves = 3, .hearts = 3, .score = 0};

    // Score screen
    ColorRect *score_screen_overlay = nullptr;
    PanelContainer *score_screen_panel = nullptr;
};

} // namespace defn

#endif
