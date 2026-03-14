#include "grid_manager.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

using godot::UtilityFunctions;

void GridManager::_bind_methods() {}

double GridManager::random_belt_y() {
    return UtilityFunctions::randf_range(BELT_TOP_Y, BELT_BOTTOM_Y);
}

double GridManager::spawn_x() {
    return SPAWN_X;
}

} // namespace defn
