// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "match_resources.h"
#include "godot_string.h"
#include "match_resource_plan.h"
#include <godot_cpp/classes/texture2d.hpp>

namespace defn {
void MatchResources::retain(const std::string &path, const godot::Ref<godot::Resource> &resource) { resources_.insert_or_assign(path, resource); }
godot::Ref<godot::Resource> MatchResources::get(const std::string &path) const {
    const auto found = resources_.find(path);
    return found == resources_.end() ? godot::Ref<godot::Resource>{} : found->second;
}
MatchResources::AnimationKey MatchResources::key(const AnimConfig &animation) {
    return {animation.path_template, animation.frame_count, animation.speed, animation.loop};
}
godot::Ref<godot::SpriteFrames> MatchResources::animation_frames(const AnimConfig &animation) const {
    const auto found = animations_.find(key(animation));
    return found == animations_.end() ? godot::Ref<godot::SpriteFrames>{} : found->second;
}
godot::Ref<godot::SpriteFrames> MatchResources::build_animation(const AnimConfig &animation) {
    if (auto cached = animation_frames(animation); cached.is_valid()) {
        return cached;
    }
    godot::Ref<godot::SpriteFrames> frames;
    frames.instantiate();
    frames->remove_animation("default");
    frames->add_animation("play");
    frames->set_animation_speed("play", animation.speed);
    frames->set_animation_loop("play", animation.loop);
    for (int frame = 0; frame < animation.frame_count; ++frame) {
        const godot::Ref<godot::Texture2D> texture = get(animation_frame_path(animation.path_template, frame));
        if (texture.is_valid()) {
            frames->add_frame("play", texture);
        }
    }
    animations_.emplace(key(animation), frames);
    return frames;
}
void MatchResources::prepare_unit(const UnitConfig &config) {
    UnitVisualFrames visual;
    visual.body.instantiate();
    visual.body->remove_animation("default");
    for (const auto &[name, animation] : config.animations) {
        const auto clip = build_animation(animation);
        const auto animation_name = to_godot_string(name);
        visual.body->add_animation(animation_name);
        visual.body->set_animation_speed(animation_name, animation.speed);
        visual.body->set_animation_loop(animation_name, animation.loop);
        for (int frame = 0; frame < clip->get_frame_count("play"); ++frame) {
            visual.body->add_frame(animation_name, clip->get_frame_texture("play", frame));
        }
    }
    if (!config.muzzle.path_template.empty()) {
        const auto clip = build_animation({.path_template = config.muzzle.path_template, .frame_count = 10, .speed = 20, .loop = false});
        visual.muzzle.instantiate();
        visual.muzzle->remove_animation("default");
        visual.muzzle->add_animation("muzzle");
        visual.muzzle->set_animation_speed("muzzle", 20);
        visual.muzzle->set_animation_loop("muzzle", false);
        for (int frame = 0; frame < clip->get_frame_count("play"); ++frame) {
            visual.muzzle->add_frame("muzzle", clip->get_frame_texture("play", frame));
        }
    }
    if (config.projectile_attack.has_value()) {
        build_animation(config.projectile_attack->projectile_animation);
        build_animation(config.projectile_attack->explosion_animation);
    }
    units_.emplace(config.name, visual);
}
UnitVisualFrames MatchResources::unit_frames(const std::string &unit_id) const {
    const auto found = units_.find(unit_id);
    return found == units_.end() ? UnitVisualFrames{} : found->second;
}
} // namespace defn
