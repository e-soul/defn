#include "game_manager.h"
#include "wave_manager.h"
#include "hud.h"
#include "grid_manager.h"
#include "defender.h"
#include "hostile.h"
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/parallax2d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace defn {

GameManager::GameManager() = default;

void GameManager::_bind_methods() {}

void GameManager::_ready() {
    UtilityFunctions::print("GameManager: Initializing belt scroller game...");

    // Setup camera and visual layers
    setup_background();
    setup_camera();

    // Entity container (y-sort so closer-to-bottom entities render in front)
    entity_container = memnew(Node2D);
    entity_container->set_name("EntityContainer");
    entity_container->set_y_sort_enabled(true);
    add_child(entity_container);

    // Wave manager
    wave_manager = memnew(WaveManager);
    wave_manager->set_name("WaveManager");
    add_child(wave_manager);

    wave_manager->load_level("res://data/levels/level_01.json");
    aether = wave_manager->get_starting_aether();
    base_integrity = wave_manager->get_base_integrity();

    wave_manager->connect("enemy_spawned", callable_mp(this, &GameManager::on_enemy_spawned));
    wave_manager->connect("wave_changed", callable_mp(this, &GameManager::on_wave_changed));
    wave_manager->connect("all_spawns_complete", callable_mp(this, &GameManager::on_all_spawns_complete));

    // HUD
    hud = memnew(HUD);
    hud->set_name("HUD");
    add_child(hud);

    hud->update_aether(aether);
    hud->update_wave(1, wave_manager->get_total_waves());
    hud->update_hearts(base_integrity);
    hud->update_deploy_button(aether >= SWORDSMAN_COST);

    // Aether generation timer: +1/sec
    aether_timer = memnew(Timer);
    aether_timer->set_wait_time(1.0);
    aether_timer->set_one_shot(false);
    aether_timer->connect("timeout", callable_mp(this, &GameManager::on_aether_tick));
    add_child(aether_timer);
    aether_timer->start();

    // Start the game
    wave_manager->start();
}

void GameManager::_process(double delta) {
    if (game_over) return;

    update_camera_scroll(delta);
    check_victory();
}

void GameManager::_input(const Ref<InputEvent> &event) {
    if (game_over) return;

    auto *mb = Object::cast_to<InputEventMouseButton>(event.ptr());
    if (!mb || !mb->is_pressed() || mb->get_button_index() != MouseButton::MOUSE_BUTTON_LEFT) {
        return;
    }

    if (aether >= SWORDSMAN_COST) {
        deploy_swordsman();
    }
}

void GameManager::setup_background() {
    auto *loader = ResourceLoader::get_singleton();

    Ref<Texture2D> bg_tex = loader->load("res://assets/backgrounds/middle_east_ruin_tiling.png");
    if (!bg_tex.is_valid()) {
        UtilityFunctions::printerr("GameManager: Failed to load tiling background");
        return;
    }

    Vector2 tex_size = bg_tex->get_size();
    double scale_factor = GridManager::VIEWPORT_HEIGHT / tex_size.y;
    double display_width = tex_size.x * scale_factor;

    world_width = display_width * GridManager::WORLD_MULTIPLIER;
    GridManager::set_world_width(world_width);

    auto *parallax = memnew(Parallax2D);
    parallax->set_name("Background");
    parallax->set_repeat_size(Vector2(display_width, 0));
    parallax->set_repeat_times(GridManager::WORLD_MULTIPLIER);
    parallax->set_scroll_scale(Vector2(1.0, 1.0));

    auto *sprite = memnew(Sprite2D);
    sprite->set_texture(bg_tex);
    sprite->set_centered(false);
    sprite->set_scale(Vector2(scale_factor, scale_factor));
    parallax->add_child(sprite);

    add_child(parallax);
}

void GameManager::setup_camera() {
    camera = memnew(Camera2D);
    camera->set_name("Camera");

    camera_target_x = GridManager::VIEWPORT_WIDTH / 2.0;
    camera->set_position(Vector2(camera_target_x, GridManager::VIEWPORT_HEIGHT / 2.0));

    camera->set_limit(SIDE_LEFT, 0);
    camera->set_limit(SIDE_TOP, 0);
    camera->set_limit(SIDE_RIGHT, static_cast<int32_t>(world_width));
    camera->set_limit(SIDE_BOTTOM, static_cast<int32_t>(GridManager::VIEWPORT_HEIGHT));

    add_child(camera);
}

void GameManager::update_camera_scroll(double delta) {
    constexpr double VIEWPORT_W = GridManager::VIEWPORT_WIDTH;
    constexpr double SCROLL_STEP = VIEWPORT_W * 0.25;

    // Trigger: any living defender within 25% of the right edge of the visible area
    double trigger_x = camera_target_x + (VIEWPORT_W / 2.0) - SCROLL_STEP;
    double max_target = world_width - VIEWPORT_W / 2.0;

    TypedArray<Node> defenders = get_tree()->get_nodes_in_group("defenders");
    for (int i = 0; i < defenders.size(); ++i) {
        auto *def = Object::cast_to<Defender>(defenders[i].operator Object *());
        if (def && !def->is_dead() && def->get_position().x > trigger_x) {
            camera_target_x += SCROLL_STEP;
            if (camera_target_x > max_target) camera_target_x = max_target;
            break;
        }
    }

    // Smooth interpolation toward the target position
    double current_x = camera->get_position().x;
    double diff = camera_target_x - current_x;
    if (diff > 1.0 || diff < -1.0) {
        constexpr double SMOOTH_FACTOR = 3.0;
        double factor = SMOOTH_FACTOR * delta;
        if (factor > 1.0) factor = 1.0;
        double new_x = current_x + diff * factor;
        camera->set_position(Vector2(new_x, GridManager::VIEWPORT_HEIGHT / 2.0));
    } else {
        camera->set_position(Vector2(camera_target_x, GridManager::VIEWPORT_HEIGHT / 2.0));
    }

    GridManager::set_camera_x(camera->get_position().x);
}

void GameManager::deploy_swordsman() {
    aether -= SWORDSMAN_COST;

    auto *swordsman = memnew(Defender);
    double x = GridManager::deploy_x();
    double y = GridManager::random_belt_y();
    swordsman->set_position(Vector2(x, y));

    swordsman->connect("entity_died", callable_mp(this, &GameManager::on_defender_died));
    entity_container->add_child(swordsman);

    hud->update_aether(aether);
    hud->update_deploy_button(aether >= SWORDSMAN_COST);
}

void GameManager::on_enemy_spawned(Node *enemy_node) {
    auto *enemy = Object::cast_to<Hostile>(enemy_node);
    if (!enemy) return;

    enemy->connect("entity_died", callable_mp(this, &GameManager::on_enemy_died));
    enemy->connect("enemy_breached", callable_mp(this, &GameManager::on_enemy_breached));
    entity_container->add_child(enemy);
    ++living_enemies;
}

void GameManager::on_wave_changed(int wave_number) {
    if (hud) {
        hud->update_wave(wave_number, wave_manager->get_total_waves());
    }
    UtilityFunctions::print(vformat("Wave %d started!", wave_number));
}

void GameManager::on_all_spawns_complete() {
    all_spawned = true;
}

void GameManager::on_enemy_died(Node *entity) {
    auto *hostile = Object::cast_to<Hostile>(entity);
    if (hostile && !hostile->is_queued_for_deletion()) {
        aether += hostile->get_bounty();
        hud->update_aether(aether);
        hud->update_deploy_button(aether >= SWORDSMAN_COST);
    }
    --living_enemies;
    if (living_enemies < 0) living_enemies = 0;
}

void GameManager::on_defender_died(Node * /*entity*/) {
    // Defender died — no special handling needed beyond removal
}

void GameManager::on_aether_tick() {
    aether += 1;
    hud->update_aether(aether);
    hud->update_deploy_button(aether >= SWORDSMAN_COST);
}

void GameManager::on_enemy_breached() {
    --base_integrity;
    --living_enemies;
    if (living_enemies < 0) living_enemies = 0;
    if (base_integrity < 0) base_integrity = 0;
    hud->update_hearts(base_integrity);

    if (base_integrity <= 0) {
        game_over = true;
        aether_timer->stop();
        hud->show_defeat();
        wave_manager->stop();
    }
}

void GameManager::check_victory() {
    if (all_spawned && living_enemies <= 0) {
        game_over = true;
        aether_timer->stop();
        hud->show_victory();
        wave_manager->stop();
    }
}

} // namespace defn
