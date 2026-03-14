#include "grid_manager.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

using godot::UtilityFunctions;

double GridManager::world_width_ = VIEWPORT_WIDTH * WORLD_MULTIPLIER;

void GridManager::_bind_methods() {}

double GridManager::random_belt_y() {
    return UtilityFunctions::randf_range(BELT_TOP_Y, BELT_BOTTOM_Y);
}

double GridManager::spawn_x() {
    return world_width_ + 100.0;
}

void GridManager::set_world_width(double w) {
    world_width_ = w;
}

double GridManager::get_world_width() {
    return world_width_;
}

} // namespace defn
