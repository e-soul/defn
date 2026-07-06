#include "projectile_rules.h"

#include <algorithm>
#include <cmath>

namespace defn {

namespace {

float distance_squared(const CombatPoint &left, const CombatPoint &right) {
    const float delta_x = left.x - right.x;
    const float delta_y = left.y - right.y;
    return (delta_x * delta_x) + (delta_y * delta_y);
}

bool contains_command(std::span<const ProjectileDamageCommand> commands, EntityId target_id) {
    return std::ranges::any_of(commands, [target_id](const ProjectileDamageCommand &command) { return command.target_id == target_id; });
}

bool is_valid_candidate(const ProjectileTargetSnapshot &target, UnitSide shooter_side) {
    return target.id.is_valid() && !target.dead && target.side != shooter_side;
}

} // namespace

int resolve_projectile_impact_damage(const ProjectileDamageConfig &config, int fallback_damage) { return config.impact_damage.value_or(fallback_damage); }

int resolve_projectile_splash_damage(const ProjectileDamageConfig &config, int fallback_damage) {
    return config.splash_damage.value_or(resolve_projectile_impact_damage(config, fallback_damage));
}

int compute_affected_projectile_target_count(const ProjectileDamageConfig &config, int candidate_count) {
    if (candidate_count <= 0) {
        return 0;
    }

    const float raw_count = static_cast<float>(candidate_count) * config.affected_fraction;
    const float rounded_count = [config, raw_count]() {
        switch (config.affected_target_rounding) {
        case SplashTargetRoundingMode::FLOOR:
            return std::floor(raw_count);
        case SplashTargetRoundingMode::NEAREST:
            return std::round(raw_count);
        case SplashTargetRoundingMode::CEIL:
            return std::ceil(raw_count);
        }

        return raw_count;
    }();

    const int rounded_targets = static_cast<int>(rounded_count);
    return std::clamp(std::max(rounded_targets, config.min_affected_targets), 1, candidate_count);
}

std::vector<ProjectileDamageCommand> resolve_projectile_impact(const ProjectileImpactInput &input) {
    std::vector<ProjectileDamageCommand> candidates;
    candidates.reserve(input.targets.size());

    if (input.config.include_direct_target && input.direct_target_id.is_valid()) {
        for (const ProjectileTargetSnapshot &target : input.targets) {
            if (target.id == input.direct_target_id && is_valid_candidate(target, input.shooter_side)) {
                candidates.push_back({.target_id = target.id, .damage = resolve_projectile_impact_damage(input.config, input.fallback_damage)});
                break;
            }
        }
    }

    const float splash_radius_squared = input.config.splash_radius * input.config.splash_radius;
    for (const ProjectileTargetSnapshot &target : input.targets) {
        if (!is_valid_candidate(target, input.shooter_side) || contains_command(candidates, target.id)) {
            continue;
        }

        if (distance_squared(target.position, input.impact_position) > splash_radius_squared) {
            continue;
        }

        const bool is_direct_target = target.id == input.direct_target_id;
        candidates.push_back({
            .target_id = target.id,
            .damage = is_direct_target ? resolve_projectile_impact_damage(input.config, input.fallback_damage)
                                      : resolve_projectile_splash_damage(input.config, input.fallback_damage),
        });
    }

    if (candidates.empty()) {
        return {};
    }

    const int affected_count = compute_affected_projectile_target_count(input.config, static_cast<int>(candidates.size()));
    candidates.resize(static_cast<size_t>(affected_count));
    return candidates;
}

} // namespace defn