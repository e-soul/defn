#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include "camera_scroll_controller.h"
#include "match_director.h"
#include "unit_data.h"
#include <cstdint>
#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/camera2d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <optional>
#include <vector>

namespace defn {

using namespace godot;

class HUD;
class BaseObjective;
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
    void setup_base_objective();
    void setup_scroll_triggers();
    Area2D *create_scroll_trigger(const String &name, uint32_t collision_mask);
    void update_scroll_trigger_positions();
    static Vector2 get_base_objective_position();
    static bool is_valid_scroll_trigger_unit(Area2D *area, const char *required_group);
    void update_camera_scroll(double delta);
    void add_friendly_unit(Unit *unit);
    void add_enemy_unit(Unit *unit);
    void refresh_resource_ui();
    void apply_match_update(const MatchUpdate &update);

    // Signal callbacks
    void on_scroll_triggered(Area2D *area, bool move_left);
    void on_enemy_died(Node *unit);
    void on_friendly_died(Node *unit);
    void on_base_durability_changed(int current_hp, int max_hp);
    void on_base_destroyed();
    void on_core_resource_tick();
    void on_deploy_requested(const String &unit_type);

    // Score screen callbacks
    void on_score_screen_next_level(const String &level_id);
    void on_score_screen_retry(const String &level_id);
    void on_score_screen_main_menu();
    void on_score_screen_upgrade_selected(const String &upgrade_id);

    // Child nodes
    HUD *hud = nullptr;
    Node2D *entity_container = nullptr;
    Timer *core_resource_timer = nullptr;
    Camera2D *camera = nullptr;
    BaseObjective *base_objective = nullptr;
    Area2D *left_scroll_trigger = nullptr;
    Area2D *right_scroll_trigger = nullptr;

    // Scrolling state
    CameraScrollController camera_scroll_controller_;

    // Unit data
    UnitDataLoader unit_data_;
    MatchDirector match_director_;
};

} // namespace defn

#endif
