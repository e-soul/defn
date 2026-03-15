#include "game_manager.h"
#include "defender.h"
#include "grid_manager.h"
#include "hostile.h"
#include "hud.h"
#include "wave_manager.h"
#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/parallax2d.hpp>
#include <godot_cpp/classes/rectangle_shape2d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

GameManager::GameManager() = default;

void GameManager::_bind_methods() {}

void GameManager::_ready() {
    UtilityFunctions::print("GameManager: Initializing belt scroller game...");

    // Setup camera and visual layers
    setup_background();
    setup_camera();
    setup_scroll_trigger();

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
    core_resource = wave_manager->get_starting_core_resource();
    base_integrity = wave_manager->get_base_integrity();

    wave_manager->connect("enemy_spawned", callable_mp(this, &GameManager::on_enemy_spawned));
    wave_manager->connect("wave_changed", callable_mp(this, &GameManager::on_wave_changed));
    wave_manager->connect("all_spawns_complete", callable_mp(this, &GameManager::on_all_spawns_complete));

    // HUD
    hud = memnew(HUD);
    hud->set_name("HUD");
    add_child(hud);

    hud->update_core_resource(core_resource);
    hud->update_wave(1, wave_manager->get_total_waves());
    hud->update_hearts(base_integrity);
    hud->update_deploy_button(core_resource >= SWORDSMAN_COST);

    // Core resource generation timer: +1/sec
    core_resource_timer = memnew(Timer);
    core_resource_timer->set_wait_time(1.0);
    core_resource_timer->set_one_shot(false);
    core_resource_timer->connect("timeout", callable_mp(this, &GameManager::on_core_resource_tick));
    add_child(core_resource_timer);
    core_resource_timer->start();

    // Start the game
    wave_manager->start();
}

void GameManager::_process(double delta) {
    if (game_over) {
        return;
    }

    update_camera_scroll(delta);
    check_victory();
}

void GameManager::_input(const Ref<InputEvent> &event) {
    if (game_over) {
        return;
    }

    auto *mouse_btn = Object::cast_to<InputEventMouseButton>(event.ptr());
    if (!mouse_btn || !mouse_btn->is_pressed() || mouse_btn->get_button_index() != MouseButton::MOUSE_BUTTON_LEFT) {
        return;
    }

    if (core_resource >= SWORDSMAN_COST) {
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
    parallax->set_repeat_size(Vector2(static_cast<real_t>(display_width), 0));
    parallax->set_repeat_times(GridManager::WORLD_MULTIPLIER);
    parallax->set_scroll_scale(Vector2(1.0, 1.0));

    auto *sprite = memnew(Sprite2D);
    sprite->set_texture(bg_tex);
    sprite->set_centered(false);
    sprite->set_scale(Vector2(static_cast<real_t>(scale_factor), static_cast<real_t>(scale_factor)));
    parallax->add_child(sprite);

    add_child(parallax);
}

void GameManager::setup_camera() {
    camera = memnew(Camera2D);
    camera->set_name("Camera");

    camera_target_x = GridManager::VIEWPORT_WIDTH / 2.0;
    camera->set_position(Vector2(static_cast<real_t>(camera_target_x), static_cast<real_t>(GridManager::VIEWPORT_HEIGHT / 2.0)));

    camera->set_limit(SIDE_LEFT, 0);
    camera->set_limit(SIDE_TOP, 0);
    camera->set_limit(SIDE_RIGHT, static_cast<int32_t>(world_width));
    camera->set_limit(SIDE_BOTTOM, static_cast<int32_t>(GridManager::VIEWPORT_HEIGHT));

    add_child(camera);
}

void GameManager::update_camera_scroll(double delta) {
    // Smooth interpolation toward the target position
    double current_x = camera->get_position().x;
    double diff = camera_target_x - current_x;
    if (diff > 1.0 || diff < -1.0) {
        constexpr double SMOOTH_FACTOR = 3.0;
        double factor = SMOOTH_FACTOR * delta;
        factor = std::min(factor, 1.0);
        double new_x = current_x + (diff * factor);
        camera->set_position(Vector2(static_cast<real_t>(new_x), static_cast<real_t>(GridManager::VIEWPORT_HEIGHT / 2.0)));
    } else {
        camera->set_position(Vector2(static_cast<real_t>(camera_target_x), static_cast<real_t>(GridManager::VIEWPORT_HEIGHT / 2.0)));
    }

    GridManager::set_camera_x(camera->get_position().x);
}

void GameManager::setup_scroll_trigger() {
    scroll_trigger = memnew(Area2D);
    scroll_trigger->set_name("ScrollTrigger");
    scroll_trigger->set_collision_layer(0); // not detectable by others
    scroll_trigger->set_collision_mask(1);  // monitors defender hitboxes (layer 1)
    scroll_trigger->set_monitoring(true);
    scroll_trigger->set_monitorable(false);

    auto *shape_node = memnew(CollisionShape2D);
    Ref<RectangleShape2D> rect;
    rect.instantiate();
    // Tall vertical strip covering well beyond the belt area
    constexpr double trigger_height = GridManager::BELT_BOTTOM_Y - GridManager::BELT_TOP_Y + 400.0;
    rect->set_size(Vector2(20.0, trigger_height));
    shape_node->set_shape(rect);
    scroll_trigger->add_child(shape_node);

    update_scroll_trigger_position();

    scroll_trigger->connect("area_entered", callable_mp(this, &GameManager::on_scroll_triggered));
    add_child(scroll_trigger);
}

void GameManager::update_scroll_trigger_position() {
    constexpr double VIEWPORT_W = GridManager::VIEWPORT_WIDTH;
    constexpr double SCROLL_STEP = VIEWPORT_W * 0.25;
    double trigger_x = camera_target_x + (VIEWPORT_W / 2.0) - SCROLL_STEP;
    double trigger_y = (GridManager::BELT_TOP_Y + GridManager::BELT_BOTTOM_Y) / 2.0;
    scroll_trigger->set_position(Vector2(static_cast<real_t>(trigger_x), static_cast<real_t>(trigger_y)));
}

void GameManager::on_scroll_triggered(Area2D *area) {
    if (game_over) {
        return;
    }

    // Verify the overlapping area belongs to a living defender
    Node *parent = area->get_parent();
    if (!parent || !parent->is_in_group("defenders")) {
        return;
    }
    auto *def = Object::cast_to<Defender>(parent);
    if (!def || def->is_dead()) {
        return;
    }

    constexpr double VIEWPORT_W = GridManager::VIEWPORT_WIDTH;
    constexpr double SCROLL_STEP = VIEWPORT_W * 0.25;
    double max_target = world_width - (VIEWPORT_W / 2.0);

    camera_target_x += SCROLL_STEP;
    camera_target_x = std::min(camera_target_x, max_target);

    update_scroll_trigger_position();
}

void GameManager::deploy_swordsman() {
    core_resource -= SWORDSMAN_COST;

    auto *swordsman = memnew(Defender);
    double spawn_x_pos = GridManager::deploy_x();
    double spawn_y_pos = GridManager::random_belt_y();
    swordsman->set_position(Vector2(static_cast<real_t>(spawn_x_pos), static_cast<real_t>(spawn_y_pos)));

    swordsman->connect("entity_died", callable_mp(this, &GameManager::on_defender_died));
    entity_container->add_child(swordsman);

    hud->update_core_resource(core_resource);
    hud->update_deploy_button(core_resource >= SWORDSMAN_COST);
}

void GameManager::on_enemy_spawned(Node *enemy_node) {
    auto *enemy = Object::cast_to<Hostile>(enemy_node);
    if (!enemy) {
        return;
    }

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

void GameManager::on_all_spawns_complete() { all_spawned = true; }

void GameManager::on_enemy_died(Node *entity) {
    auto *hostile = Object::cast_to<Hostile>(entity);
    if (hostile && !hostile->is_queued_for_deletion()) {
        core_resource += hostile->get_bounty();
        hud->update_core_resource(core_resource);
        hud->update_deploy_button(core_resource >= SWORDSMAN_COST);
    }
    --living_enemies;
    living_enemies = std::max(living_enemies, 0);
}

void GameManager::on_defender_died(Node * /*entity*/) {
    // Defender died — no special handling needed beyond removal
}

void GameManager::on_core_resource_tick() {
    core_resource += 1;
    hud->update_core_resource(core_resource);
    hud->update_deploy_button(core_resource >= SWORDSMAN_COST);
}

void GameManager::on_enemy_breached() {
    --base_integrity;
    --living_enemies;
    living_enemies = std::max(living_enemies, 0);
    base_integrity = std::max(base_integrity, 0);
    hud->update_hearts(base_integrity);

    if (base_integrity <= 0) {
        game_over = true;
        core_resource_timer->stop();
        hud->show_defeat();
        wave_manager->stop();
    }
}

void GameManager::check_victory() {
    if (all_spawned && living_enemies <= 0) {
        game_over = true;
        core_resource_timer->stop();
        hud->show_victory();
        wave_manager->stop();
    }
}

} // namespace defn
