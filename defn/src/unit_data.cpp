#include "unit_data.h"
#include "variant_tools.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

constexpr real_t DEFAULT_ALPHA = 1.0;
constexpr real_t DEFAULT_HEALTH_BAR_OFFSET_X = 0.0;
constexpr real_t DEFAULT_HEALTH_BAR_OFFSET_Y = -241.0;
constexpr real_t LEGACY_MOVE_SPEED_SCALE = 128.0F;

bool parse_json_file(const String &path, Variant &out_data) {
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (!file.is_valid()) {
        UtilityFunctions::printerr("UnitDataLoader: Failed to open: ", path);
        return false;
    }

    String json_text = file->get_as_text();
    file->close();

    Ref<JSON> json;
    json.instantiate();
    const Error err = json->parse(json_text);
    if (err != OK) {
        UtilityFunctions::printerr("UnitDataLoader: JSON parse error in ", path, ": ", json->get_error_message());
        return false;
    }

    out_data = json->get_data();
    return true;
}

Color parse_color(const Array &arr, const Color &fallback) {
    if (arr.size() >= 3) {
        const auto red = VariantTools::as_real(arr[0]);
        const auto green = VariantTools::as_real(arr[1]);
        const auto blue = VariantTools::as_real(arr[2]);
        const auto alpha = arr.size() >= 4 ? VariantTools::as_real(arr[3]) : DEFAULT_ALPHA;
        return {red, green, blue, alpha};
    }
    return fallback;
}

Vector2 parse_vector2(const Variant &value, const Vector2 &fallback) {
    Array arr = value;
    if (arr.size() >= 2) {
        return {VariantTools::as_real(arr[0]), VariantTools::as_real(arr[1])};
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

UnitSide parse_unit_side(const Dictionary &unit_dict) {
    const String side_str = String(unit_dict.get("side", "friendly"));
    return side_str == "hostile" ? UnitSide::HOSTILE : UnitSide::FRIENDLY;
}

void apply_unit_colors(const Dictionary &unit_dict, const GlobalUnitConfig &globals, UnitConfig &config) {
    const Color default_health_bar_color = config.side == UnitSide::HOSTILE ? globals.hostile_health_bar_color : globals.friendly_health_bar_color;
    const Color default_melee_flash_color = config.side == UnitSide::HOSTILE ? globals.hostile_melee_flash_color : globals.friendly_melee_flash_color;
    const Color default_ranged_flash_color = config.side == UnitSide::HOSTILE ? globals.hostile_ranged_flash_color : globals.friendly_ranged_flash_color;

    config.health_bar_color = parse_color(unit_dict.get("health_bar_color", Array()), default_health_bar_color);
    config.health_bar_offset = parse_vector2(unit_dict.get("health_bar_offset", Array()), Vector2(DEFAULT_HEALTH_BAR_OFFSET_X, DEFAULT_HEALTH_BAR_OFFSET_Y));
    config.melee_flash_color = parse_color(unit_dict.get("melee_flash_color", Array()), default_melee_flash_color);
    config.ranged_flash_color = parse_color(unit_dict.get("ranged_flash_color", Array()), default_ranged_flash_color);
}

void apply_muzzle_flash(const Dictionary &unit_dict, UnitConfig &config) {
    if (!unit_dict.has("muzzle_flash")) {
        return;
    }

    Dictionary muzzle_flash_dict = unit_dict["muzzle_flash"];
    config.muzzle.path_template = String(muzzle_flash_dict.get("path_template", ""));
    Array offset = muzzle_flash_dict.get("offset", Array());
    if (offset.size() >= 2) {
        config.muzzle.offset = Vector2(VariantTools::as_real(offset[0]), VariantTools::as_real(offset[1]));
    }
    config.muzzle.flip_h = VariantTools::as_bool(muzzle_flash_dict.get("flip_h", false));
}

void apply_shoot_sfx(const Dictionary &unit_dict, const GlobalUnitConfig &globals, UnitConfig &config) {
    if (unit_dict.has("shoot_sfx")) {
        Dictionary shoot_sfx = unit_dict["shoot_sfx"];
        config.shoot_sfx.path = String(shoot_sfx.get("path", ""));
        config.shoot_sfx.volume_linear = VariantTools::as_float(shoot_sfx.get("volume_linear", 1.0));
        config.shoot_sfx.pitch_scale = VariantTools::as_float(shoot_sfx.get("pitch_scale", 1.0));
    }

    config.shoot_sfx.pitch_variance = globals.shoot_sfx.pitch_variance;
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
        AnimConfig animation;
        animation.path_template = String(animation_dict.get("path_template", ""));
        animation.frame_count = VariantTools::as_int(animation_dict.get("frame_count", 10));
        animation.speed = VariantTools::as_double(animation_dict.get("speed", 10.0));
        animation.loop = VariantTools::as_bool(animation_dict.get("loop", false));
        config.animations.emplace_back(animation_name, animation);
    }
}

UnitConfig parse_unit_config(const String &key, const Dictionary &unit_dict, const GlobalUnitConfig &globals) {
    UnitConfig config;
    config.name = key;
    config.side = parse_unit_side(unit_dict);
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
        config.move_speed_pixels_per_second = VariantTools::as_real(unit_dict.get("move_speed_pixels_per_second", config.move_speed_pixels_per_second));
    } else {
        config.move_speed_pixels_per_second = VariantTools::as_real(unit_dict.get("move_speed", 0.5)) * LEGACY_MOVE_SPEED_SCALE;
    }
    config.cost = VariantTools::as_int(unit_dict.get("cost", 0));
    config.bounty = VariantTools::as_int(unit_dict.get("bounty", 0));
    config.scale = VariantTools::as_real(unit_dict.get("scale", 0.27));
    config.sprite_flip_h = VariantTools::as_bool(unit_dict.get("sprite_flip_h", false));

    apply_unit_colors(unit_dict, globals, config);
    apply_muzzle_flash(unit_dict, config);
    apply_shoot_sfx(unit_dict, globals, config);
    apply_animations(unit_dict, config);

    return config;
}

} // namespace

bool UnitDataLoader::load(const String &unit_path, const String &global_path) {
    Variant global_data_variant;
    if (!parse_json_file(global_path, global_data_variant)) {
        return false;
    }

    Dictionary global_data = global_data_variant;
    globals_ = {};
    load_global_config(global_data, globals_);

    Variant unit_data_variant;
    if (!parse_json_file(unit_path, unit_data_variant)) {
        return false;
    }

    Dictionary data = unit_data_variant;
    Dictionary units_dict = data.get("units", Dictionary());
    Array keys = units_dict.keys();
    units_.clear();

    for (const Variant &key_variant : keys) {
        const String key = key_variant;
        const Dictionary unit_dict = units_dict[key];
        units_.push_back(parse_unit_config(key, unit_dict, globals_));
    }

    UtilityFunctions::print("UnitDataLoader: Loaded ", units_.size(), " unit types");
    return true;
}

std::optional<UnitConfig> UnitDataLoader::get_unit(const String &name) const {
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
