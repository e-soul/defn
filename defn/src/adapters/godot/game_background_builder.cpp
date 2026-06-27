#include "game_background_builder.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

GameBackgroundBuildResult GameBackgroundBuilder::build(const String &background_path, const GameplayRules &rules) {
    auto *loader = ResourceLoader::get_singleton();
    Ref<Texture2D> background_texture = loader->load(background_path);
    if (!background_texture.is_valid()) {
        UtilityFunctions::printerr("GameBackgroundBuilder: Failed to load background: ", background_path);
        return {};
    }

    const Vector2 texture_size = background_texture->get_size();
    const real_t scale_factor = rules.viewport_height / texture_size.y;
    const real_t display_width = texture_size.x * scale_factor;

    auto *parallax = memnew(Parallax2D);
    parallax->set_name("Background");
    parallax->set_repeat_size(Vector2(display_width, 0));
    parallax->set_repeat_times(rules.world_multiplier);
    parallax->set_scroll_scale(Vector2(1.0, 1.0));

    auto *sprite = memnew(Sprite2D);
    sprite->set_texture(background_texture);
    sprite->set_centered(false);
    sprite->set_scale(Vector2(scale_factor, scale_factor));
    parallax->add_child(sprite);

    return {
        .background = parallax,
        .world_width = display_width * static_cast<real_t>(rules.world_multiplier),
    };
}

} // namespace defn