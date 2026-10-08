// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "game_background_builder.h"

#include "godot_string.h"

#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>
#include <optional>
#include <vector>

namespace defn {

namespace {
struct BackgroundVisual {
    Node2D *node;
    godot::Vector2 source_size;
};

std::optional<BackgroundVisual> load_background_visual(const String &path) {
    auto *loader = ResourceLoader::get_singleton();
    if (path.get_extension() == "tscn") {
        const Ref<PackedScene> scene = loader->load(path);
        if (scene.is_valid()) {
            auto *instance = scene->instantiate();
            auto *visual = Object::cast_to<Node2D>(instance);
            const godot::Vector2 size = visual == nullptr ? godot::Vector2() : godot::Vector2(visual->get_meta("source_size", godot::Vector2()));
            if (visual != nullptr && size.x > 0 && size.y > 0) {
                return BackgroundVisual{.node = visual, .source_size = size};
            }
            memdelete(instance);
        }
    } else {
        const Ref<Texture2D> texture = loader->load(path);
        if (texture.is_valid()) {
            auto *sprite = memnew(Sprite2D);
            sprite->set_texture(texture);
            sprite->set_centered(false);
            return BackgroundVisual{.node = sprite, .source_size = texture->get_size()};
        }
    }
    UtilityFunctions::printerr("GameBackgroundBuilder: Failed to load background: ", path);
    return std::nullopt;
}
} // namespace

Parallax2D *GameBackgroundBuilder::build(const String &background_path, const GameplayRules &rules) {
    const auto visual = load_background_visual(background_path);
    if (!visual) {
        return nullptr;
    }

    const godot::Vector2 texture_size = visual->source_size;
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

    visual->node->set_scale(Vector2(scale_factor, scale_factor));
    parallax->add_child(visual->node);

    return parallax;
}

Node2D *GameBackgroundBuilder::build_stack(const std::vector<BackgroundLayer> &layers, const GameplayRules &rules) {
    auto *root = memnew(Node2D);
    root->set_name("Background");

    int drawn = 0;
    for (const BackgroundLayer &layer : layers) {
        const auto visual = load_background_visual(to_godot_string(layer.path));
        if (!visual) {
            continue;
        }

        const godot::Vector2 texture_size = visual->source_size;
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

        visual->node->set_scale(Vector2(scale_factor, scale_factor));
        visual->node->set_position(Vector2(0, (rules.viewport_height * layer.bottom_ratio) - display_height));
        parallax->add_child(visual->node);

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
