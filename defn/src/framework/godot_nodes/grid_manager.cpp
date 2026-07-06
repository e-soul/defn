#include "grid_manager.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

using godot::UtilityFunctions;

GridManager *GridManager::singleton_ = nullptr;

void GridManager::_bind_methods() {
    ClassDB::bind_static_method(get_class_static(), D_METHOD("random_belt_y"), &GridManager::random_belt_y);
    ClassDB::bind_method(D_METHOD("deploy_x"), &GridManager::deploy_x);
    ClassDB::bind_method(D_METHOD("spawn_x"), &GridManager::spawn_x);
    ClassDB::bind_method(D_METHOD("set_world_width", "width"), &GridManager::set_world_width);
    ClassDB::bind_method(D_METHOD("get_world_width"), &GridManager::get_world_width);
    ClassDB::bind_method(D_METHOD("set_camera_x", "camera_x"), &GridManager::set_camera_x);
    ClassDB::bind_method(D_METHOD("get_camera_x"), &GridManager::get_camera_x);
}

GridManager *GridManager::get_singleton() { return singleton_; }

void GridManager::register_singleton() {
    if (singleton_ != nullptr) {
        return;
    }

    singleton_ = memnew(GridManager);
    Engine::get_singleton()->register_singleton("GridManager", singleton_);
}

void GridManager::unregister_singleton() {
    if (singleton_ == nullptr) {
        return;
    }

    Engine::get_singleton()->unregister_singleton("GridManager");
    memdelete(singleton_);
    singleton_ = nullptr;
}

void GridManager::configure(const GameplayRules &rules) {
    rules_ = rules;
    world_width_ = rules_.viewport_width * static_cast<real_t>(rules_.world_multiplier);
    camera_x_ = rules_.viewport_width / 2.0F;
}

real_t GridManager::random_belt_y() {
    static const GameplayRules default_rules{};
    const GameplayRules &rules = singleton_ != nullptr ? singleton_->rules_ : default_rules;
    return static_cast<real_t>(UtilityFunctions::randf_range(rules.belt_top_y, rules.belt_bottom_y));
}

double GridManager::deploy_x() const { return camera_x_ - (rules_.viewport_width / 2.0F) - rules_.spawn_offset; }

double GridManager::spawn_x() const { return camera_x_ + (rules_.viewport_width / 2.0F) + rules_.spawn_offset; }

double GridManager::sample_belt_y() const { return random_belt_y(); }

void GridManager::set_world_width(real_t width) { world_width_ = width; }

real_t GridManager::get_world_width() const { return world_width_; }

void GridManager::set_camera_x(real_t camera_x) { camera_x_ = camera_x; }

real_t GridManager::get_camera_x() const { return camera_x_; }

} // namespace defn
