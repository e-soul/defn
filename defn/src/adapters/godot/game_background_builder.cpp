// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "game_background_builder.h"

#include "godot_string.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>
#include <vector>

namespace defn {

Parallax2D *GameBackgroundBuilder::build(const String &background_path, const GameplayRules &rules) {
    auto *loader = ResourceLoader::get_singleton();
    Ref<Texture2D> background_texture = loader->load(background_path);
    if (!background_texture.is_valid()) {
        UtilityFunctions::printerr("GameBackgroundBuilder: Failed to load background: ", background_path);
        return nullptr;
    }

    const Vector2 texture_size = background_texture->get_size();
    const real_t scale_factor = rules.viewport_height / texture_size.y;
    const real_t display_width = texture_size.x * scale_factor;

    // Enough tiles to span the screen wherever the camera has scrolled to, and not one more: the repeat is a drawing
    // window that travels with the camera, not the size of the world.
    const auto repeat_times = static_cast<int32_t>(std::ceil(rules.viewport_width / display_width)) + REPEAT_MARGIN;

    auto *parallax = memnew(Parallax2D);
    parallax->set_name("Background");
    parallax->set_repeat_size(Vector2(display_width, 0));
    parallax->set_repeat_times(repeat_times);
    parallax->set_scroll_scale(Vector2(1.0, 1.0));

    auto *sprite = memnew(Sprite2D);
    sprite->set_texture(background_texture);
    sprite->set_centered(false);
    sprite->set_scale(Vector2(scale_factor, scale_factor));
    parallax->add_child(sprite);

    return parallax;
}

Node2D *GameBackgroundBuilder::build_stack(const std::vector<BackgroundLayer> &layers, const GameplayRules &rules) {
    auto *root = memnew(Node2D);
    root->set_name("Background");

    auto *loader = ResourceLoader::get_singleton();
    int drawn = 0;
    for (const BackgroundLayer &layer : layers) {
        Ref<Texture2D> texture = loader->load(to_godot_string(layer.path));
        if (!texture.is_valid()) {
            UtilityFunctions::printerr("GameBackgroundBuilder: Failed to load background layer: ", to_godot_string(layer.path));
            continue;
        }

        const Vector2 texture_size = texture->get_size();
        // Every layer is placed by fractions of viewport height, so each one's stored resolution is its own
        // business: the sky is authored at half the density of the floor and lands in exactly the right place.
        const real_t display_height = rules.viewport_height * layer.height_ratio;
        const real_t scale_factor = display_height / texture_size.y;
        const real_t display_width = texture_size.x * scale_factor;

        auto *parallax = memnew(Parallax2D);
        parallax->set_name(vformat("Layer%d", drawn));
        parallax->set_repeat_size(Vector2(display_width, 0));
        parallax->set_repeat_times(static_cast<int32_t>(std::ceil(rules.viewport_width / display_width)) + REPEAT_MARGIN);
        // Horizontal only. A vertical rate would slide the planes against each other as the camera moves down,
        // and the camera's vertical travel is not depth -- it is the same ground seen from a different height.
        parallax->set_scroll_scale(Vector2(layer.scroll_scale, 1.0));
        // Drift is horizontal only and independent of the camera, so a cloud plane keeps moving through the
        // long stretches where the front line is not advancing.
        parallax->set_autoscroll(Vector2(layer.autoscroll, 0.0));

        auto *sprite = memnew(Sprite2D);
        sprite->set_texture(texture);
        sprite->set_centered(false);
        sprite->set_scale(Vector2(scale_factor, scale_factor));
        sprite->set_position(Vector2(0, (rules.viewport_height * layer.bottom_ratio) - display_height));
        parallax->add_child(sprite);

        // Tree order is draw order, and the list is authored back to front, so appending is all the depth
        // sorting this needs.
        root->add_child(parallax);
        ++drawn;
    }

    if (drawn == 0) {
        UtilityFunctions::printerr("GameBackgroundBuilder: No background layer could be loaded");
        memdelete(root);
        return nullptr;
    }
    return root;
}

} // namespace defn