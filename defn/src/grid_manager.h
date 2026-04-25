#ifndef GRID_MANAGER_H
#define GRID_MANAGER_H

#include "gameplay_rules.h"
#include "runtime_service_interfaces.h"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

namespace defn {

using namespace godot;

class GridManager : public Object, public GridQueryService {
    GDCLASS(GridManager, Object)

  public:
    GridManager() = default;

    static GridManager *get_singleton();
    static void register_singleton();
    static void unregister_singleton();

    void configure(const GameplayRules &rules);
    const GameplayRules &get_rules() const { return rules_; }

    [[nodiscard]] static real_t random_belt_y();    // random Y within the belt area
    [[nodiscard]] real_t deploy_x() const override; // friendly spawn: just left of camera
    [[nodiscard]] real_t spawn_x() const override;  // hostile spawn: just right of camera
    [[nodiscard]] real_t sample_belt_y() const override;

    void set_world_width(real_t w);
    real_t get_world_width() const;
    void set_camera_x(real_t x);
    real_t get_camera_x() const;

  protected:
    static void _bind_methods();

  private:
    static GridManager *singleton_;

    GameplayRules rules_{};
    real_t world_width_ = rules_.viewport_width * static_cast<real_t>(rules_.world_multiplier);
    real_t camera_x_ = rules_.viewport_width / 2.0F;
};

} // namespace defn

#endif
