#include "unit_data.h"
#include "godot_string.h"
#include "json_file_loader.h"
#include "variant_tools.h"

#include <algorithm>

#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

constexpr float DEFAULT_ALPHA = 1.0F;
constexpr float DEFAULT_HEALTH_BAR_OFFSET_X = 0.0F;
constexpr float DEFAULT_HEALTH_BAR_OFFSET_Y = -241.0F;
constexpr float LEGACY_MOVE_SPEED_SCALE = 128.0F;
constexpr float FRACTION_PERCENT_SCALE = 100.0F;

Color parse_color(const Array &arr, const Color &fallback) {
    if (arr.size() >= 3) {
        const auto red = VariantTools::as_float(arr[0]);
        const auto green = VariantTools::as_float(arr[1]);
        const auto blue = VariantTools::as_float(arr[2]);
        const auto alpha = arr.size() >= 4 ? VariantTools::as_float(arr[3]) : DEFAULT_ALPHA;
        return {.r = red, .g = green, .b = blue, .a = alpha};
    }
    return fallback;
}

ContentVector2 parse_vector2(const Variant &value, const ContentVector2 &fallback) {
    Array arr = value;
    if (arr.size() >= 2) {
        return {.x = VariantTools::as_float(arr[0]), .y = VariantTools::as_float(arr[1])};
    }
    return fallback;
}

RangeVariationConfig parse_range_variation(const Variant &value, const RangeVariationConfig &fallback) {
    Array arr = value;
    if (arr.size() >= 2) {
        return {
            .min = VariantTools::as_real(arr[0]),
            .max = VariantTools::as_real(arr[1]),
        };
    }
    return fallback;
}

AnimConfig parse_anim_config(const Dictionary &animation_dict, const AnimConfig &fallback) {
    AnimConfig animation = fallback;
    animation.path_template = to_std_string(String(animation_dict.get("path_template", to_godot_string(animation.path_template))));
    animation.frame_count = VariantTools::as_int(animation_dict.get("frame_count", animation.frame_count));
    animation.speed = VariantTools::as_double(animation_dict.get("speed", animation.speed));
    animation.loop = VariantTools::as_bool(animation_dict.get("loop", animation.loop));
    return animation;
}

SplashTargetRoundingMode parse_splash_target_rounding_mode_impl(const Variant &value, SplashTargetRoundingMode fallback) {
    const String mode = String(value).to_lower();
    if (mode == "floor") {
        return SplashTargetRoundingMode::FLOOR;
    }
    if (mode == "ceil") {
        return SplashTargetRoundingMode::CEIL;
    }
    if (mode == "nearest") {
        return SplashTargetRoundingMode::NEAREST;
    }
    return fallback;
}

float normalize_fraction(float value) {
    if (value > 1.0F) {
        value /= FRACTION_PERCENT_SCALE;
    }
    return std::clamp(value, 0.0F, 1.0F);
}

void load_global_config(const Dictionary &global_data, GlobalUnitConfig &globals) {
    if (global_data.has("gameplay_rules")) {
        const Dictionary gameplay_rules = global_data["gameplay_rules"];
        globals.gameplay_rules.viewport_width = VariantTools::as_real(gameplay_rules.get("viewport_width", globals.gameplay_rules.viewport_width));
        globals.gameplay_rules.viewport_height = VariantTools::as_real(gameplay_rules.get("viewport_height", globals.gameplay_rules.viewport_height));
        globals.gameplay_rules.world_multiplier = VariantTools::as_int(gameplay_rules.get("world_multiplier", globals.gameplay_rules.world_multiplier));
        globals.gameplay_rules.belt_top_y = VariantTools::as_real(gameplay_rules.get("belt_top_y", globals.gameplay_rules.belt_top_y));
        globals.gameplay_rules.belt_bottom_y = VariantTools::as_real(gameplay_rules.get("belt_bottom_y", globals.gameplay_rules.belt_bottom_y));
        globals.gameplay_rules.breach_x = VariantTools::as_real(gameplay_rules.get("breach_x", globals.gameplay_rules.breach_x));
        globals.gameplay_rules.spawn_offset = VariantTools::as_real(gameplay_rules.get("spawn_offset", globals.gameplay_rules.spawn_offset));
        globals.gameplay_rules.friendly_world_margin =
            VariantTools::as_real(gameplay_rules.get("friendly_world_margin", globals.gameplay_rules.friendly_world_margin));
        globals.gameplay_rules.scroll_trigger_extra_height =
            VariantTools::as_real(gameplay_rules.get("scroll_trigger_extra_height", globals.gameplay_rules.scroll_trigger_extra_height));
        globals.gameplay_rules.camera_scroll_step_factor =
            VariantTools::as_real(gameplay_rules.get("camera_scroll_step_factor", globals.gameplay_rules.camera_scroll_step_factor));
    }

    if (global_data.has("shoot_sfx")) {
        Dictionary global_sfx = global_data["shoot_sfx"];
        globals.shoot_sfx.pitch_variance = VariantTools::as_float(global_sfx.get("pitch_variance", 0.0));
    }

    if (global_data.has("attack_range_variation")) {
        Dictionary global_range_variation = global_data["attack_range_variation"];
        globals.melee_attack_range_variation = parse_range_variation(global_range_variation.get("melee", Array()), globals.melee_attack_range_variation);
        globals.ranged_attack_range_variation = parse_range_variation(global_range_variation.get("ranged", Array()), globals.ranged_attack_range_variation);
    }

    if (global_data.has("health_bar_color")) {
        Dictionary global_health_bar_colors = global_data["health_bar_color"];
        globals.friendly_health_bar_color = parse_color(global_health_bar_colors.get("friendly", Array()), globals.friendly_health_bar_color);
        globals.hostile_health_bar_color = parse_color(global_health_bar_colors.get("hostile", Array()), globals.hostile_health_bar_color);
    }

    if (global_data.has("melee_flash_color")) {
        Dictionary global_melee_flash_colors = global_data["melee_flash_color"];
        globals.friendly_melee_flash_color = parse_color(global_melee_flash_colors.get("friendly", Array()), globals.friendly_melee_flash_color);
        globals.hostile_melee_flash_color = parse_color(global_melee_flash_colors.get("hostile", Array()), globals.hostile_melee_flash_color);
    }

    if (global_data.has("ranged_flash_color")) {
        Dictionary global_ranged_flash_colors = global_data["ranged_flash_color"];
        globals.friendly_ranged_flash_color = parse_color(global_ranged_flash_colors.get("friendly", Array()), globals.friendly_ranged_flash_color);
        globals.hostile_ranged_flash_color = parse_color(global_ranged_flash_colors.get("hostile", Array()), globals.hostile_ranged_flash_color);
    }
}

UnitSide parse_unit_side_impl(const Dictionary &unit_dict) {
    const String side_str = String(unit_dict.get("side", "friendly"));
    return side_str == "hostile" ? UnitSide::HOSTILE : UnitSide::FRIENDLY;
}

void apply_unit_colors(const Dictionary &unit_dict, const GlobalUnitConfig &globals, UnitConfig &config) {
    const Color default_health_bar_color = config.side == UnitSide::HOSTILE ? globals.hostile_health_bar_color : globals.friendly_health_bar_color;
    const Color default_melee_flash_color = config.side == UnitSide::HOSTILE ? globals.hostile_melee_flash_color : globals.friendly_melee_flash_color;
    const Color default_ranged_flash_color = config.side == UnitSide::HOSTILE ? globals.hostile_ranged_flash_color : globals.friendly_ranged_flash_color;

    config.health_bar_color = parse_color(unit_dict.get("health_bar_color", Array()), default_health_bar_color);
    config.health_bar_offset = parse_vector2(unit_dict.get("health_bar_offset", Array()), {.x = DEFAULT_HEALTH_BAR_OFFSET_X, .y = DEFAULT_HEALTH_BAR_OFFSET_Y});
    config.melee_flash_color = parse_color(unit_dict.get("melee_flash_color", Array()), default_melee_flash_color);
    config.ranged_flash_color = parse_color(unit_dict.get("ranged_flash_color", Array()), default_ranged_flash_color);
}

void apply_muzzle_flash(const Dictionary &unit_dict, UnitConfig &config) {
    if (!unit_dict.has("muzzle_flash")) {
        return;
    }

    Dictionary muzzle_flash_dict = unit_dict["muzzle_flash"];
    config.muzzle.path_template = to_std_string(String(muzzle_flash_dict.get("path_template", "")));
    Array offset = muzzle_flash_dict.get("offset", Array());
    if (offset.size() >= 2) {
        config.muzzle.offset = {.x = VariantTools::as_float(offset[0]), .y = VariantTools::as_float(offset[1])};
    }
    config.muzzle.flip_h = VariantTools::as_bool(muzzle_flash_dict.get("flip_h", false));
}

void apply_shoot_sfx(const Dictionary &unit_dict, const GlobalUnitConfig &globals, UnitConfig &config) {
    if (unit_dict.has("shoot_sfx")) {
        Dictionary shoot_sfx = unit_dict["shoot_sfx"];
        config.shoot_sfx.path = to_std_string(String(shoot_sfx.get("path", "")));
        config.shoot_sfx.volume_linear = VariantTools::as_float(shoot_sfx.get("volume_linear", 1.0));
        config.shoot_sfx.pitch_scale = VariantTools::as_float(shoot_sfx.get("pitch_scale", 1.0));
    }

    config.shoot_sfx.pitch_variance = globals.shoot_sfx.pitch_variance;
}

ShootSfxConfig parse_shoot_sfx_config(const Dictionary &shoot_sfx_dict, const ShootSfxConfig &fallback) {
    ShootSfxConfig config = fallback;
    config.path = to_std_string(String(shoot_sfx_dict.get("path", to_godot_string(config.path))));
    config.volume_linear = VariantTools::as_float(shoot_sfx_dict.get("volume_linear", config.volume_linear));
    config.pitch_scale = VariantTools::as_float(shoot_sfx_dict.get("pitch_scale", config.pitch_scale));
    config.pitch_variance = VariantTools::as_float(shoot_sfx_dict.get("pitch_variance", config.pitch_variance));
    return config;
}

void apply_projectile_attack(const Dictionary &unit_dict, UnitConfig &config) {
    if (!unit_dict.has("projectile_attack")) {
        return;
    }

    const Dictionary projectile_dict = unit_dict["projectile_attack"];
    ProjectileAttackConfig projectile;
    projectile.speed_pixels_per_second = VariantTools::as_real(projectile_dict.get("speed_pixels_per_second", projectile.speed_pixels_per_second));
    projectile.splash_radius = VariantTools::as_real(projectile_dict.get("splash_radius", projectile.splash_radius));
    projectile.affected_fraction = normalize_fraction(VariantTools::as_float(projectile_dict.get("affected_fraction", projectile.affected_fraction)));
    projectile.min_affected_targets = std::max(1, VariantTools::as_int(projectile_dict.get("min_affected_targets", projectile.min_affected_targets)));
    projectile.spawn_animation_frame = std::max(0, VariantTools::as_int(projectile_dict.get("spawn_animation_frame", projectile.spawn_animation_frame)));
    projectile.affected_target_rounding =
        parse_splash_target_rounding_mode_impl(projectile_dict.get("affected_target_rounding", Variant("nearest")), projectile.affected_target_rounding);
    projectile.include_direct_target = VariantTools::as_bool(projectile_dict.get("include_direct_target", projectile.include_direct_target));
    projectile.projectile_scale_multiplier = VariantTools::as_real(projectile_dict.get("projectile_scale_multiplier", projectile.projectile_scale_multiplier));
    projectile.explosion_scale_multiplier = VariantTools::as_real(projectile_dict.get("explosion_scale_multiplier", projectile.explosion_scale_multiplier));

    if (projectile_dict.has("impact_damage")) {
        projectile.impact_damage = VariantTools::as_int(projectile_dict.get("impact_damage", 0));
    }
    if (projectile_dict.has("splash_damage")) {
        projectile.splash_damage = VariantTools::as_int(projectile_dict.get("splash_damage", 0));
    }
    if (projectile_dict.has("explosion_sfx")) {
        projectile.explosion_sfx = parse_shoot_sfx_config(projectile_dict["explosion_sfx"], ShootSfxConfig{});
    }

    if (projectile_dict.has("animation")) {
        projectile.projectile_animation = parse_anim_config(projectile_dict["animation"], projectile.projectile_animation);
    }
    if (projectile_dict.has("explosion_animation")) {
        projectile.explosion_animation = parse_anim_config(projectile_dict["explosion_animation"], projectile.explosion_animation);
    }

    config.projectile_attack = projectile;
}

void apply_animations(const Dictionary &unit_dict, UnitConfig &config) {
    if (!unit_dict.has("animations")) {
        return;
    }

    Dictionary animations = unit_dict["animations"];
    Array animation_keys = animations.keys();
    for (const Variant &animation_key : animation_keys) {
        String animation_name = animation_key;
        Dictionary animation_dict = animations[animation_name];
        config.animations.emplace_back(to_std_string(animation_name), parse_anim_config(animation_dict, AnimConfig{}));
    }
}

UnitConfig parse_unit_config(const String &key, const Dictionary &unit_dict, const GlobalUnitConfig &globals) {
    UnitConfig config;
    config.name = to_std_string(key);
    config.side = parse_unit_side_impl(unit_dict);
    config.hp = VariantTools::as_int(unit_dict.get("hp", 100));
    config.melee_damage = VariantTools::as_int(unit_dict.get("melee_damage", 15));
    config.melee_attack_period_seconds = VariantTools::as_double(unit_dict.get("melee_attack_period_seconds", 1.0));
    config.melee_attack_range = VariantTools::as_real(unit_dict.get("melee_attack_range", config.melee_attack_range));
    config.melee_attack_range_variation = globals.melee_attack_range_variation;
    config.ranged_damage = VariantTools::as_int(unit_dict.get("ranged_damage", 8));
    config.ranged_attack_period_seconds = VariantTools::as_double(unit_dict.get("ranged_attack_period_seconds", 2.0 / 3.0));
    config.ranged_attack_range = VariantTools::as_real(unit_dict.get("ranged_attack_range", config.ranged_attack_range));
    config.ranged_attack_range_variation = globals.ranged_attack_range_variation;
    if (unit_dict.has("move_speed_pixels_per_second")) {
        config.move_speed_pixels_per_second = VariantTools::as_float(unit_dict.get("move_speed_pixels_per_second", config.move_speed_pixels_per_second));
    } else {
        config.move_speed_pixels_per_second = VariantTools::as_float(unit_dict.get("move_speed", 0.5)) * LEGACY_MOVE_SPEED_SCALE;
    }
    config.cost = VariantTools::as_int(unit_dict.get("cost", 0));
    config.bounty = VariantTools::as_int(unit_dict.get("bounty", 0));
    config.scale = VariantTools::as_real(unit_dict.get("scale", 0.27));
    config.sprite_flip_h = VariantTools::as_bool(unit_dict.get("sprite_flip_h", false));

    apply_unit_colors(unit_dict, globals, config);
    apply_muzzle_flash(unit_dict, config);
    apply_shoot_sfx(unit_dict, globals, config);
    apply_projectile_attack(unit_dict, config);
    apply_animations(unit_dict, config);

    return config;
}

} // namespace

UnitSide parse_unit_side(const Dictionary &unit_dict) { return parse_unit_side_impl(unit_dict); }

SplashTargetRoundingMode parse_splash_target_rounding_mode(const Variant &value, SplashTargetRoundingMode fallback) {
    return parse_splash_target_rounding_mode_impl(value, fallback);
}

bool UnitDataLoader::load(const String &unit_path, const String &global_path) {
    const auto global_data = JsonFileLoader::load_dictionary(global_path, "UnitDataLoader");
    if (!global_data) {
        return false;
    }

    const auto unit_data = JsonFileLoader::load_dictionary(unit_path, "UnitDataLoader");
    if (!unit_data) {
        return false;
    }

    const bool loaded = load_from_data(*unit_data, *global_data);
    if (loaded) {
        UtilityFunctions::print("UnitDataLoader: Loaded ", units_.size(), " unit types");
    }
    return loaded;
}

bool UnitDataLoader::load_from_data(const Dictionary &unit_data, const Dictionary &global_data) {
    globals_ = {};
    load_global_config(global_data, globals_);

    Dictionary units_dict = unit_data.get("units", Dictionary());
    Array keys = units_dict.keys();
    units_.clear();

    for (const Variant &key_variant : keys) {
        const String key = key_variant;
        const Dictionary unit_dict = units_dict[key];
        units_.push_back(parse_unit_config(key, unit_dict, globals_));
    }

    return true;
}

std::optional<UnitConfig> UnitDataLoader::get_unit(const std::string &name) const {
    for (const auto &cfg : units_) {
        if (cfg.name == name) {
            return cfg;
        }
    }
    return std::nullopt;
}

std::vector<UnitConfig> UnitDataLoader::get_friendly_units() const {
    std::vector<UnitConfig> result;
    for (const auto &cfg : units_) {
        if (cfg.side == UnitSide::FRIENDLY) {
            result.push_back(cfg);
        }
    }
    return result;
}

} // namespace defn
