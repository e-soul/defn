#ifndef UNIT_DATA_H
#define UNIT_DATA_H

#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <optional>
#include <unordered_map>
#include <vector>

namespace defn {

using namespace godot;

enum class UnitSide { FRIENDLY, HOSTILE };

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

struct GlobalUnitConfig {
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
    real_t move_speed = 0.5;
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
    std::vector<std::pair<String, AnimConfig>> animations;
};

class UnitDataLoader {
  public:
    bool load(const String &unit_path, const String &global_path);

    std::optional<UnitConfig> get_unit(const String &name) const;
    std::vector<UnitConfig> get_friendly_units() const;

    const GlobalUnitConfig &get_globals() const { return globals_; }

  private:
    GlobalUnitConfig globals_;
    std::vector<UnitConfig> units_;
};

} // namespace defn

#endif
