#ifndef GRID_MANAGER_H
#define GRID_MANAGER_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace defn {

using namespace godot;

class GridManager : public Object {
    GDCLASS(GridManager, Object)

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

    GridManager() = default;

    static GridManager *get_singleton();
    static void register_singleton();
    static void unregister_singleton();

    static double random_belt_y(); // random Y within the belt area
    double deploy_x() const;      // defender spawn: just left of camera
    double spawn_x() const;       // hostile spawn: just right of camera

    void set_world_width(double w);
    double get_world_width() const;
    void set_camera_x(double x);
    double get_camera_x() const;

  protected:
    static void _bind_methods();

  private:
    static GridManager *singleton_;

    double world_width_ = VIEWPORT_WIDTH * WORLD_MULTIPLIER;
    double camera_x_ = VIEWPORT_WIDTH / 2.0;
};

} // namespace defn

#endif
