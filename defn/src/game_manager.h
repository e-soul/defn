#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/timer.hpp>
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
    void deploy_swordsman();

    // Signal callbacks
    void on_enemy_spawned(Node *enemy_node);
    void on_wave_changed(int wave_number);
    void on_all_spawns_complete();
    void on_enemy_died(Node *entity);
    void on_defender_died(Node *entity);
    void on_enemy_breached();
    void on_aether_tick();

    void check_victory();

    // Game state
    int aether = 100;
    int base_integrity = 3;
    bool game_over = false;
    bool all_spawned = false;

    // Tracking living entities
    int living_enemies = 0;

    // Child nodes
    WaveManager *wave_manager = nullptr;
    HUD *hud = nullptr;
    Node2D *entity_container = nullptr;
    Timer *aether_timer = nullptr;

    static constexpr int SWORDSMAN_COST = 25;
};

} // namespace defn

#endif
