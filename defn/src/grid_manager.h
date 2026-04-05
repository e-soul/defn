#ifndef GRID_MANAGER_H
#define GRID_MANAGER_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

namespace defn {

using namespace godot;

class GridManager : public Object {
    GDCLASS(GridManager, Object)

  public:
    // Viewport & world geometry
    static constexpr real_t VIEWPORT_WIDTH = 1920.0;
    static constexpr real_t VIEWPORT_HEIGHT = 1080.0;
    static constexpr int WORLD_MULTIPLIER = 4; // world is N× the background width

    // Belt scroller constants
    static constexpr real_t BELT_TOP_Y = 750.0;    // top of walkable belt
    static constexpr real_t BELT_BOTTOM_Y = 850.0; // bottom of walkable belt
    static constexpr real_t BREACH_X = 50.0;       // breach threshold
    static constexpr real_t ATTACK_RANGE = 128.0;  // melee attack range in pixels
    static constexpr real_t RANGED_RANGE = 384.0;  // ranged attack range in pixels
    static constexpr real_t SPAWN_OFFSET = 100.0;  // pixels off-screen for spawning

    GridManager() = default;

    static GridManager *get_singleton();
    static void register_singleton();
    static void unregister_singleton();

    static real_t random_belt_y(); // random Y within the belt area
    real_t deploy_x() const;       // friendly spawn: just left of camera
    real_t spawn_x() const;        // hostile spawn: just right of camera

    void set_world_width(real_t w);
    real_t get_world_width() const;
    void set_camera_x(real_t x);
    real_t get_camera_x() const;

  protected:
    static void _bind_methods();

  private:
    static GridManager *singleton_;

    real_t world_width_ = VIEWPORT_WIDTH * WORLD_MULTIPLIER;
    real_t camera_x_ = VIEWPORT_WIDTH / 2.0;
};

} // namespace defn

#endif
