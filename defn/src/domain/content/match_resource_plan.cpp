// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "match_resource_plan.h"
#include <algorithm>
#include <charconv>
#include <set>

namespace defn {
std::vector<std::string> match_unit_ids(const LevelDefinition &level, std::span<const std::string> friendly_ids, const EndlessSchedule *endless) {
    std::set<std::string> ids{"base"};
    ids.insert(friendly_ids.begin(), friendly_ids.end());
    for (const auto &wave : level.waves) {
        for (const auto &spawn : wave.spawns) {
            ids.insert(spawn.type);
        }
    }
    if (endless != nullptr) {
        EndlessWaveGenerator generator;
        generator.configure(*endless);
        const int final_wave = generator.final_wave();
        for (int wave = 1; wave <= final_wave; ++wave) {
            for (const auto &entry : generator.composition(wave)) {
                if (entry.count > 0) {
                    ids.insert(entry.unit_id);
                }
            }
        }
    }
    return {ids.begin(), ids.end()};
}

std::string animation_frame_path(const std::string &path_template, int frame) {
    const auto start = path_template.find('%');
    if (start == std::string::npos) {
        return path_template;
    }
    auto end = start + 1;
    while (end < path_template.size() && path_template[end] >= '0' && path_template[end] <= '9') {
        ++end;
    }
    if (end == path_template.size() || path_template[end] != 'd') {
        return path_template;
    }
    int width = 0;
    if (end > start + 1) {
        std::from_chars(path_template.data() + start + 1, path_template.data() + end, width);
    }
    std::string number = std::to_string(frame);
    if (width > 0 && width <= 16 && static_cast<std::size_t>(width) > number.size()) {
        number.insert(0, static_cast<std::size_t>(width) - number.size(), path_template[start + 1] == '0' ? '0' : ' ');
    }
    return path_template.substr(0, start) + number + path_template.substr(end + 1);
}

std::vector<std::string> match_resource_paths(const LevelDefinition &level, std::span<const UnitConfig> units,
                                              std::span<const std::string> presentation_paths) {
    std::set<std::string> paths;
    const auto add = [&](const std::string &path) {
        if (!path.empty()) {
            paths.insert(path);
        }
    };
    const auto add_animation = [&](const AnimConfig &animation) {
        if (!animation.path_template.empty()) {
            for (int frame = 0; frame < animation.frame_count; ++frame) {
                add(animation_frame_path(animation.path_template, frame));
            }
        }
    };
    if (level.background_layers.empty()) {
        add(level.background_path);
    } else {
        for (const auto &layer : level.background_layers) {
            add(layer.path);
        }
    }
    for (const auto &unit : units) {
        for (const auto &[name, animation] : unit.animations) {
            add_animation(animation);
        }
        if (!unit.muzzle.path_template.empty()) {
            add_animation({.path_template = unit.muzzle.path_template, .frame_count = 10});
        }
        add(unit.shoot_sfx.path);
        if (unit.projectile_attack.has_value()) {
            add_animation(unit.projectile_attack->projectile_animation);
            add_animation(unit.projectile_attack->explosion_animation);
            if (unit.projectile_attack->explosion_sfx.has_value()) {
                add(unit.projectile_attack->explosion_sfx->path);
            }
        }
    }
    for (const auto &path : presentation_paths) {
        add(path);
    }
    return {paths.begin(), paths.end()};
}
} // namespace defn
