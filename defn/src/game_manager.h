#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include "camera_scroll_controller.h"
#include "match_session.h"
#include "unit_data.h"
#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/camera2d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <vector>

namespace defn {

using namespace godot;

class WaveManager;
class HUD;
class Unit;

class GameManager : public Node2D {
    GDCLASS(GameManager, Node2D)

  public:
    GameManager();

    void _ready() override;
    void _process(double delta) override;
    void _input(const Ref<InputEvent> &event) override;

  protected:
    static void _bind_methods();

  private:
    void setup_background(const String &bg_path);
    void setup_camera();
    void setup_scroll_trigger();
    void update_scroll_trigger_position();
    void deploy_friendly(const String &unit_type);
    void update_camera_scroll(double delta);

    // Signal callbacks
    void on_scroll_triggered(Area2D *area);
    void on_enemy_spawned(Node *enemy_node);
    void on_wave_changed(int wave_number);
    void on_all_spawns_complete();
    void on_enemy_died(Node *unit);
    void on_friendly_died(Node *unit);
    void on_enemy_breached();
    void on_core_resource_tick();
    void on_deploy_requested(const String &unit_type);

    // Score screen callbacks
    void on_score_screen_next_level(const String &level_id);
    void on_score_screen_retry(const String &level_id);
    void on_score_screen_main_menu();

    void check_victory();
    void end_game(bool victory);

    MatchSession match_session_;

    // Child nodes
    WaveManager *wave_manager = nullptr;
    HUD *hud = nullptr;
    Node2D *entity_container = nullptr;
    Timer *core_resource_timer = nullptr;
    Camera2D *camera = nullptr;
    Area2D *scroll_trigger = nullptr;

    // Scrolling state
    CameraScrollController camera_scroll_controller_;

    // Unit data
    UnitDataLoader unit_data_;
};

} // namespace defn

#endif
