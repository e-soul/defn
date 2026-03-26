#include "unit_data.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

float as_float(const Variant &value) {
    return static_cast<float>(static_cast<double>(value));
}

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
        const auto red = as_float(arr[0]);
        const auto green = as_float(arr[1]);
        const auto blue = as_float(arr[2]);
        const auto alpha = arr.size() >= 4 ? as_float(arr[3]) : 1.0F;
        return {red, green, blue, alpha};
    }
    return fallback;
}

} // namespace

bool UnitDataLoader::load(const String &unit_path, const String &global_path) {
    Variant global_data_variant;
    if (!parse_json_file(global_path, global_data_variant)) {
        return false;
    }

    Dictionary global_data = global_data_variant;
    globals_ = {};

    if (global_data.has("shoot_sfx")) {
        Dictionary global_sfx = global_data["shoot_sfx"];
        globals_.shoot_sfx.pitch_variance = static_cast<double>(global_sfx.get("pitch_variance", 0.0));
    }

    Variant unit_data_variant;
    if (!parse_json_file(unit_path, unit_data_variant)) {
        return false;
    }

    Dictionary data = unit_data_variant;
    Dictionary units_dict = data.get("units", Dictionary());
    Array keys = units_dict.keys();
    units_.clear();

    for (const Variant &key_variant : keys) {
        String key = key_variant;
        Dictionary unit_dict = units_dict[key];

        UnitConfig cfg;
        cfg.name = key;

        String side_str = String(unit_dict.get("side", "friendly"));
        cfg.side = (side_str == "hostile") ? UnitSide::HOSTILE : UnitSide::FRIENDLY;

        cfg.hp = static_cast<int>(unit_dict.get("hp", 100));
        cfg.melee_damage = static_cast<int>(unit_dict.get("melee_damage", 15));
        cfg.melee_attack_speed = static_cast<double>(unit_dict.get("melee_attack_speed", 1.0));
        cfg.ranged_damage = static_cast<int>(unit_dict.get("ranged_damage", 8));
        cfg.ranged_attack_speed = static_cast<double>(unit_dict.get("ranged_attack_speed", 1.5));
        cfg.move_speed = static_cast<double>(unit_dict.get("move_speed", 0.5));
        cfg.cost = static_cast<int>(unit_dict.get("cost", 0));
        cfg.bounty = static_cast<int>(unit_dict.get("bounty", 0));
        cfg.scale = static_cast<double>(unit_dict.get("scale", 0.27));
        cfg.sprite_flip_h = static_cast<bool>(unit_dict.get("sprite_flip_h", false));

        cfg.health_bar_color = parse_color(unit_dict.get("health_bar_color", Array()), Color(0, 1, 0, 0.9));
        cfg.melee_flash_color = parse_color(unit_dict.get("melee_flash_color", Array()), Color(1, 1, 1));
        cfg.ranged_flash_color = parse_color(unit_dict.get("ranged_flash_color", Array()), Color(1, 1, 1));

        // Muzzle flash
        if (unit_dict.has("muzzle_flash")) {
            Dictionary muzzle_flash_dict = unit_dict["muzzle_flash"];
            cfg.muzzle.path_template = String(muzzle_flash_dict.get("path_template", ""));
            Array offset = muzzle_flash_dict.get("offset", Array());
            if (offset.size() >= 2) {
                cfg.muzzle.offset = Vector2(as_float(offset[0]), as_float(offset[1]));
            }
            cfg.muzzle.flip_h = static_cast<bool>(muzzle_flash_dict.get("flip_h", false));
        }

        if (unit_dict.has("shoot_sfx")) {
            Dictionary sfx = unit_dict["shoot_sfx"];
            cfg.shoot_sfx.path = String(sfx.get("path", ""));
            cfg.shoot_sfx.volume_linear = static_cast<double>(sfx.get("volume_linear", 1.0));
            cfg.shoot_sfx.pitch_scale = static_cast<double>(sfx.get("pitch_scale", 1.0));
        }

        cfg.shoot_sfx.pitch_variance = globals_.shoot_sfx.pitch_variance;

        // Animations
        if (unit_dict.has("animations")) {
            Dictionary anims = unit_dict["animations"];
            Array anim_keys = anims.keys();
            for (const Variant &anim_key : anim_keys) {
                String anim_name = anim_key;
                Dictionary anim_dict = anims[anim_name];
                AnimConfig anim;
                anim.path_template = String(anim_dict.get("path_template", ""));
                anim.frame_count = static_cast<int>(anim_dict.get("frame_count", 10));
                anim.speed = static_cast<double>(anim_dict.get("speed", 10.0));
                anim.loop = static_cast<bool>(anim_dict.get("loop", false));
                cfg.animations.emplace_back(anim_name, anim);
            }
        }

        units_.push_back(cfg);
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

} // namespace defn
