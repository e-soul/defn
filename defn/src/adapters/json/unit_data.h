#ifndef UNIT_DATA_H
#define UNIT_DATA_H

#include "combat_types.h"
#include "gameplay_rules.h"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

namespace defn {

using namespace godot;

struct AnimConfig {
    String path_template;
    int frame_count = 10;
    double speed = 10.0;
    bool loop = false;
};

struct MuzzleConfig {
    String path_template;
    Vector2 offset;
    bool flip_h = false;
};

struct ShootSfxConfig {
    String path;
    float volume_linear = 1.0F;
    float pitch_scale = 1.0F;
    float pitch_variance = 0.0F;
};

struct GlobalShootSfxConfig {
    float pitch_variance = 0.0F;
};

struct RangeVariationConfig {
    real_t min = 0.8;
    real_t max = 1.2;
};

struct ProjectileAttackConfig {
    real_t speed_pixels_per_second = 0.0F;
    real_t splash_radius = 0.0F;
    real_t affected_fraction = 1.0F;
    int min_affected_targets = 1;
    int spawn_animation_frame = 0;
    SplashTargetRoundingMode affected_target_rounding = SplashTargetRoundingMode::NEAREST;
    bool include_direct_target = true;
    std::optional<int> impact_damage;
    std::optional<int> splash_damage;
    real_t projectile_scale_multiplier = 1.0F;
    real_t explosion_scale_multiplier = 1.0F;
    std::optional<ShootSfxConfig> explosion_sfx;
    AnimConfig projectile_animation;
    AnimConfig explosion_animation;
};

struct GlobalUnitConfig {
    GameplayRules gameplay_rules;
    GlobalShootSfxConfig shoot_sfx;
    RangeVariationConfig melee_attack_range_variation;
    RangeVariationConfig ranged_attack_range_variation;
    Color friendly_health_bar_color = Color(0, 1, 0, 0.9);
    Color hostile_health_bar_color = Color(1, 0, 0, 0.9);
    Color friendly_melee_flash_color = Color(1, 1, 1);
    Color hostile_melee_flash_color = Color(1, 1, 1);
    Color friendly_ranged_flash_color = Color(1, 1, 1);
    Color hostile_ranged_flash_color = Color(1, 1, 1);
};

struct UnitConfig {
    String name;
    UnitSide side = UnitSide::FRIENDLY;
    int hp = 100;
    int melee_damage = 15;
    double melee_attack_period_seconds = 1.0;
    real_t melee_attack_range = 128.0;
    RangeVariationConfig melee_attack_range_variation;
    int ranged_damage = 8;
    double ranged_attack_period_seconds = 2.0 / 3.0;
    real_t ranged_attack_range = 384.0;
    RangeVariationConfig ranged_attack_range_variation;
    real_t move_speed_pixels_per_second = 64.0F;
    int cost = 0;
    int bounty = 0;
    real_t scale = 0.27;
    bool sprite_flip_h = false;
    Color health_bar_color = Color(0, 1, 0, 0.9);
    Vector2 health_bar_offset = Vector2(0.0, -241.0);
    Color melee_flash_color = Color(1, 1, 1);
    Color ranged_flash_color = Color(1, 1, 1);
    MuzzleConfig muzzle;
    ShootSfxConfig shoot_sfx;
    std::optional<ProjectileAttackConfig> projectile_attack;
    std::vector<std::pair<String, AnimConfig>> animations;
};

UnitSide parse_unit_side(const Dictionary &unit_dict);
SplashTargetRoundingMode parse_splash_target_rounding_mode(const Variant &value, SplashTargetRoundingMode fallback);
ProjectileDamageConfig to_projectile_damage_config(const ProjectileAttackConfig &config);

class UnitCatalog {
    public:
        virtual ~UnitCatalog() = default;

        [[nodiscard]] virtual std::optional<UnitConfig> get_unit(const String &name) const = 0;
        [[nodiscard]] virtual std::vector<UnitConfig> get_friendly_units() const = 0;
};

class UnitDataLoader : public UnitCatalog {
  public:
    bool load(const String &unit_path, const String &global_path);
    bool load_from_data(const Dictionary &unit_data, const Dictionary &global_data);

        [[nodiscard]] std::optional<UnitConfig> get_unit(const String &name) const override;
        [[nodiscard]] std::vector<UnitConfig> get_friendly_units() const override;

    const GlobalUnitConfig &get_globals() const { return globals_; }

  private:
    GlobalUnitConfig globals_;
    std::vector<UnitConfig> units_;
};

} // namespace defn

#endif
