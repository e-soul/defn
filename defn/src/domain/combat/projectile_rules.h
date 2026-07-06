#ifndef PROJECTILE_RULES_H
#define PROJECTILE_RULES_H

#include "combat_types.h"

#include <span>
#include <vector>

namespace defn {

struct ProjectileTargetSnapshot {
    EntityId id;
    UnitSide side = UnitSide::FRIENDLY;
    bool dead = false;
    CombatPoint position;
};

struct ProjectileDamageCommand {
    EntityId target_id;
    int damage = 0;
};

struct ProjectileImpactInput {
    ProjectileDamageConfig config;
    UnitSide shooter_side = UnitSide::FRIENDLY;
    CombatPoint impact_position;
    EntityId direct_target_id;
    int fallback_damage = 0;
    std::span<const ProjectileTargetSnapshot> targets;
};

int resolve_projectile_impact_damage(const ProjectileDamageConfig &config, int fallback_damage);
int resolve_projectile_splash_damage(const ProjectileDamageConfig &config, int fallback_damage);
int compute_affected_projectile_target_count(const ProjectileDamageConfig &config, int candidate_count);
std::vector<ProjectileDamageCommand> resolve_projectile_impact(const ProjectileImpactInput &input);

} // namespace defn

#endif