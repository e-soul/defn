#include "grid_manager.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

using godot::UtilityFunctions;

double GridManager::world_width_ = VIEWPORT_WIDTH * WORLD_MULTIPLIER;
double GridManager::camera_x_ = VIEWPORT_WIDTH / 2.0;

void GridManager::_bind_methods() {}

double GridManager::random_belt_y() { return UtilityFunctions::randf_range(BELT_TOP_Y, BELT_BOTTOM_Y); }

double GridManager::deploy_x() { return camera_x_ - (VIEWPORT_WIDTH / 2.0) - SPAWN_OFFSET; }

double GridManager::spawn_x() { return camera_x_ + (VIEWPORT_WIDTH / 2.0) + SPAWN_OFFSET; }

void GridManager::set_world_width(double width) { world_width_ = width; }

double GridManager::get_world_width() { return world_width_; }

void GridManager::set_camera_x(double camera_x) { camera_x_ = camera_x; }

double GridManager::get_camera_x() { return camera_x_; }

} // namespace defn
