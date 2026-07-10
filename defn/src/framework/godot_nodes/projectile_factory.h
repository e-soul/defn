#ifndef PROJECTILE_FACTORY_H
#define PROJECTILE_FACTORY_H

#include "unit_definition.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace defn {

using namespace godot;

class AttackTarget;
class ProjectileAttack;

class ProjectileFactory {
  public:
    ProjectileFactory() = delete;

    static ProjectileAttack *create(Node *parent, const ProjectileAttackConfig &config, UnitSide shooter_side, const godot::Color &flash_color,
                                    const godot::Vector2 &start_global_position, const godot::Vector2 &target_global_position, AttackTarget *direct_target,
                                    int fallback_damage);
};

} // namespace defn

#endif