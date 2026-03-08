#include "grid_manager.h"

namespace defn {

void GridManager::_bind_methods() {}

double GridManager::lane_center_y(int lane) {
    return GRID_ORIGIN_Y + (lane - 1) * TILE_SIZE + TILE_SIZE * 0.5;
}

double GridManager::column_center_x(int col) {
    return GRID_ORIGIN_X + col * TILE_SIZE + TILE_SIZE * 0.5;
}

int GridManager::screen_to_lane(double screen_y) {
    double relative_y = screen_y - GRID_ORIGIN_Y;
    if (relative_y < 0.0 || relative_y >= LANE_COUNT * TILE_SIZE) {
        return -1; // invalid
    }
    return static_cast<int>(relative_y / TILE_SIZE) + 1; // 1-indexed
}

double GridManager::spawn_x() {
    return GRID_ORIGIN_X + COLUMN_COUNT * TILE_SIZE + TILE_SIZE; // one tile past the grid
}

} // namespace defn
