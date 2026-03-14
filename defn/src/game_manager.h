#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/camera2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <vector>

namespace defn {

using namespace godot;

class WaveManager;
class HUD;
class Hostile;
class Defender;

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
    void setup_background();
    void setup_base_visual();
    void setup_camera();
    void setup_scroll_trigger();
    void update_scroll_trigger_position();
    void deploy_swordsman();
    void update_camera_scroll(double delta);

    // Signal callbacks
    void on_scroll_triggered(Area2D *area);
    void on_enemy_spawned(Node *enemy_node);
    void on_wave_changed(int wave_number);
    void on_all_spawns_complete();
    void on_enemy_died(Node *entity);
    void on_defender_died(Node *entity);
    void on_enemy_breached();
    void on_core_resource_tick();

    void check_victory();

    // Game state
    int core_resource = 100;
    int base_integrity = 3;
    bool game_over = false;
    bool all_spawned = false;

    // Tracking living entities
    int living_enemies = 0;

    // Child nodes
    WaveManager *wave_manager = nullptr;
    HUD *hud = nullptr;
    Node2D *entity_container = nullptr;
    Timer *core_resource_timer = nullptr;
    Camera2D *camera = nullptr;
    Area2D *scroll_trigger = nullptr;

    // Scrolling state
    double camera_target_x = 960.0;
    double world_width = 7680.0;

    static constexpr int SWORDSMAN_COST = 25;
};

} // namespace defn

#endif
