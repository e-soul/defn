#ifndef GRID_MANAGER_H
#define GRID_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

class GridManager : public Node {
    GDCLASS(GridManager, Node)

public:
    // Belt scroller constants (viewport 1920x1080)
    static constexpr double BELT_TOP_Y = 650.0;     // top of walkable belt
    static constexpr double BELT_BOTTOM_Y = 800.0;  // bottom of walkable belt
    static constexpr double DEPLOY_X = 150.0;       // left edge where defenders deploy
    static constexpr double BREACH_X = 50.0;        // breach threshold
    static constexpr double SPAWN_X = 2000.0;       // right edge spawn (just off-screen)
    static constexpr double ATTACK_RANGE = 128.0;   // melee attack range in pixels

    static double random_belt_y();                   // random Y within the belt area
    static double spawn_x();                         // right edge spawn position

protected:
    static void _bind_methods();
};

} // namespace defn

#endif
