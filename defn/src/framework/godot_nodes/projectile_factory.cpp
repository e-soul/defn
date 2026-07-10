#include "projectile_factory.h"

#include "projectile_attack.h"

#include <godot_cpp/core/memory.hpp>

namespace defn {

ProjectileAttack *ProjectileFactory::create(Node *parent, const ProjectileAttackConfig &config, UnitSide shooter_side, const godot::Color &flash_color,
                                            const godot::Vector2 &start_global_position, const godot::Vector2 &target_global_position,
                                            AttackTarget *direct_target, int fallback_damage) {
    auto *projectile = memnew(ProjectileAttack);
    if (parent != nullptr) {
        parent->add_child(projectile);
    }
    projectile->configure(config, shooter_side, flash_color, start_global_position, target_global_position, direct_target, fallback_damage);
    return projectile;
}

} // namespace defn