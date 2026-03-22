#include "unit_data.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

Color parse_color(const Array &arr, const Color &fallback) {
    if (arr.size() >= 3) {
        float r = static_cast<float>(static_cast<double>(arr[0]));
        float g = static_cast<float>(static_cast<double>(arr[1]));
        float b = static_cast<float>(static_cast<double>(arr[2]));
        float a = arr.size() >= 4 ? static_cast<float>(static_cast<double>(arr[3])) : 1.0f;
        return Color(r, g, b, a);
    }
    return fallback;
}

} // namespace

bool UnitDataLoader::load(const String &path) {
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (!file.is_valid()) {
        UtilityFunctions::printerr("UnitDataLoader: Failed to open: ", path);
        return false;
    }

    String json_text = file->get_as_text();
    file->close();

    Ref<JSON> json;
    json.instantiate();
    Error err = json->parse(json_text);
    if (err != OK) {
        UtilityFunctions::printerr("UnitDataLoader: JSON parse error: ", json->get_error_message());
        return false;
    }

    Dictionary data = json->get_data();
    Dictionary units_dict = data.get("units", Dictionary());
    Array keys = units_dict.keys();
    units_.clear();

    for (int i = 0; i < keys.size(); ++i) {
        String key = keys[i];
        Dictionary d = units_dict[key];

        UnitConfig cfg;
        cfg.name = key;

        String side_str = String(d.get("side", "friendly"));
        cfg.side = (side_str == "hostile") ? UnitSide::HOSTILE : UnitSide::FRIENDLY;

        cfg.hp = static_cast<int>(d.get("hp", 100));
        cfg.melee_damage = static_cast<int>(d.get("melee_damage", 15));
        cfg.melee_attack_speed = static_cast<double>(d.get("melee_attack_speed", 1.0));
        cfg.ranged_damage = static_cast<int>(d.get("ranged_damage", 8));
        cfg.ranged_attack_speed = static_cast<double>(d.get("ranged_attack_speed", 1.5));
        cfg.move_speed = static_cast<double>(d.get("move_speed", 0.5));
        cfg.cost = static_cast<int>(d.get("cost", 0));
        cfg.bounty = static_cast<int>(d.get("bounty", 0));
        cfg.scale = static_cast<double>(d.get("scale", 0.27));
        cfg.sprite_flip_h = static_cast<bool>(d.get("sprite_flip_h", false));

        cfg.health_bar_color = parse_color(d.get("health_bar_color", Array()), Color(0, 1, 0, 0.9));
        cfg.melee_flash_color = parse_color(d.get("melee_flash_color", Array()), Color(1, 1, 1));
        cfg.ranged_flash_color = parse_color(d.get("ranged_flash_color", Array()), Color(1, 1, 1));

        // Muzzle flash
        if (d.has("muzzle_flash")) {
            Dictionary mf = d["muzzle_flash"];
            cfg.muzzle.path_template = String(mf.get("path_template", ""));
            Array offset = mf.get("offset", Array());
            if (offset.size() >= 2) {
                cfg.muzzle.offset = Vector2(static_cast<float>(static_cast<double>(offset[0])), static_cast<float>(static_cast<double>(offset[1])));
            }
            cfg.muzzle.flip_h = static_cast<bool>(mf.get("flip_h", false));
        }

        if (d.has("shoot_sfx")) {
            Dictionary sfx = d["shoot_sfx"];
            cfg.shoot_sfx.path = String(sfx.get("path", ""));
        }

        // Animations
        if (d.has("animations")) {
            Dictionary anims = d["animations"];
            Array anim_keys = anims.keys();
            for (int j = 0; j < anim_keys.size(); ++j) {
                String anim_name = anim_keys[j];
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
