#ifndef GRID_MANAGER_H
#define GRID_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

class GridManager : public Node {
    GDCLASS(GridManager, Node)

public:
    static constexpr int LANE_COUNT = 5;
    static constexpr int COLUMN_COUNT = 10;
    static constexpr double TILE_SIZE = 128.0;
    static constexpr double GRID_ORIGIN_X = 288.0;  // ~15% of 1920
    static constexpr double GRID_ORIGIN_Y = 64.0;   // top bar height
    static constexpr double BASE_X_THRESHOLD = GRID_ORIGIN_X; // breach threshold

    static double lane_center_y(int lane);  // lane is 1-indexed
    static double column_center_x(int col); // col is 0-indexed
    static int screen_to_lane(double screen_y);
    static double spawn_x(); // right edge spawn position

protected:
    static void _bind_methods();
};

} // namespace defn

#endif
