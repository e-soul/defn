#ifndef GRID_MANAGER_H
#define GRID_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

class GridManager : public Node {
    GDCLASS(GridManager, Node)

  public:
    // Viewport & world geometry
    static constexpr double VIEWPORT_WIDTH = 1920.0;
    static constexpr double VIEWPORT_HEIGHT = 1080.0;
    static constexpr int WORLD_MULTIPLIER = 4; // world is N× the background width

    // Belt scroller constants
    static constexpr double BELT_TOP_Y = 750.0;    // top of walkable belt
    static constexpr double BELT_BOTTOM_Y = 850.0; // bottom of walkable belt
    static constexpr double BREACH_X = 50.0;       // breach threshold
    static constexpr double ATTACK_RANGE = 128.0;  // melee attack range in pixels
    static constexpr double RANGED_RANGE = 384.0;  // ranged attack range in pixels
    static constexpr double SPAWN_OFFSET = 100.0;  // pixels off-screen for spawning

    static double random_belt_y(); // random Y within the belt area
    static double deploy_x();      // defender spawn: just left of camera
    static double spawn_x();       // hostile spawn: just right of camera

    static void set_world_width(double w);
    static double get_world_width();
    static void set_camera_x(double x);
    static double get_camera_x();

  protected:
    static void _bind_methods();

  private:
    static double world_width_;
    static double camera_x_;
};

} // namespace defn

#endif
