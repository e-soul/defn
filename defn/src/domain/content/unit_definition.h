#ifndef UNIT_DEFINITION_H
#define UNIT_DEFINITION_H

#include "combat_types.h"
#include "content_values.h"
#include "gameplay_rules.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace defn {

struct AnimConfig {
    std::string path_template;
    int frame_count = 10;
    double speed = 10.0;
    bool loop = false;
};

struct MuzzleConfig {
    std::string path_template;
    ContentVector2 offset;
    bool flip_h = false;
};

struct ShootSfxConfig {
    std::string path;
    float volume_linear = 1.0F;
    float pitch_scale = 1.0F;
    float pitch_variance = 0.0F;
};

struct GlobalShootSfxConfig {
    float pitch_variance = 0.0F;
};

struct RangeVariationConfig {
    float min = 0.8F;
    float max = 1.2F;
};

struct ProjectileAttackConfig {
    float speed_pixels_per_second = 0.0F;
    float splash_radius = 0.0F;
    float affected_fraction = 1.0F;
    int min_affected_targets = 1;
    int spawn_animation_frame = 0;
    SplashTargetRoundingMode affected_target_rounding = SplashTargetRoundingMode::NEAREST;
    bool include_direct_target = true;
    std::optional<int> impact_damage;
    std::optional<int> splash_damage;
    float projectile_scale_multiplier = 1.0F;
    float explosion_scale_multiplier = 1.0F;
    std::optional<ShootSfxConfig> explosion_sfx;
    AnimConfig projectile_animation;
    AnimConfig explosion_animation;
};

inline ProjectileDamageConfig to_projectile_damage_config(const ProjectileAttackConfig &config) {
    return {
        .splash_radius = config.splash_radius,
        .affected_fraction = config.affected_fraction,
        .min_affected_targets = config.min_affected_targets,
        .affected_target_rounding = config.affected_target_rounding,
        .include_direct_target = config.include_direct_target,
        .impact_damage = config.impact_damage,
        .splash_damage = config.splash_damage,
    };
}

struct GlobalUnitConfig {
    GameplayRules gameplay_rules;
    GlobalShootSfxConfig shoot_sfx;
    RangeVariationConfig melee_attack_range_variation;
    RangeVariationConfig ranged_attack_range_variation;
    Color friendly_health_bar_color = {0.0F, 1.0F, 0.0F, 0.9F};
    Color hostile_health_bar_color = {1.0F, 0.0F, 0.0F, 0.9F};
    Color friendly_melee_flash_color = {1.0F, 1.0F, 1.0F, 1.0F};
    Color hostile_melee_flash_color = {1.0F, 1.0F, 1.0F, 1.0F};
    Color friendly_ranged_flash_color = {1.0F, 1.0F, 1.0F, 1.0F};
    Color hostile_ranged_flash_color = {1.0F, 1.0F, 1.0F, 1.0F};
};

struct UnitConfig {
    std::string name;
    UnitSide side = UnitSide::FRIENDLY;
    int hp = 100;
    int melee_damage = 15;
    double melee_attack_period_seconds = 1.0;
    float melee_attack_range = 128.0F;
    RangeVariationConfig melee_attack_range_variation;
    int ranged_damage = 8;
    double ranged_attack_period_seconds = 2.0 / 3.0;
    float ranged_attack_range = 384.0F;
    RangeVariationConfig ranged_attack_range_variation;
    float move_speed_pixels_per_second = 64.0F;
    int cost = 0;
    int bounty = 0;
    float scale = 0.27F;
    bool sprite_flip_h = false;
    Color health_bar_color = {0.0F, 1.0F, 0.0F, 0.9F};
    ContentVector2 health_bar_offset = {0.0F, -241.0F};
    Color melee_flash_color = {1.0F, 1.0F, 1.0F, 1.0F};
    Color ranged_flash_color = {1.0F, 1.0F, 1.0F, 1.0F};
    MuzzleConfig muzzle;
    ShootSfxConfig shoot_sfx;
    std::optional<ProjectileAttackConfig> projectile_attack;
    std::vector<std::pair<std::string, AnimConfig>> animations;
};

class UnitCatalog {
  public:
    virtual ~UnitCatalog() = default;

    [[nodiscard]] virtual std::optional<UnitConfig> get_unit(const std::string &name) const = 0;
    [[nodiscard]] virtual std::vector<UnitConfig> get_friendly_units() const = 0;
};

} // namespace defn

#endif