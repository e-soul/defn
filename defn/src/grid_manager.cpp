#include "grid_manager.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <cstdlib>

namespace defn {

void GridManager::_bind_methods() {}

double GridManager::random_belt_y() {
    double t = static_cast<double>(std::rand()) / RAND_MAX;
    return BELT_TOP_Y + t * (BELT_BOTTOM_Y - BELT_TOP_Y);
}

double GridManager::spawn_x() {
    return SPAWN_X;
}

} // namespace defn
