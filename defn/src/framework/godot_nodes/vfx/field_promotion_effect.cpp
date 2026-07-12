#include "field_promotion_effect.h"

#include <algorithm>
#include <cmath>

#include <godot_cpp/classes/cpu_particles2d.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>

namespace defn::vfx {

namespace {

constexpr int PARTICLE_COUNT = 28;
constexpr int PARTICLE_TEXTURE_SIZE = 32;
constexpr double PARTICLE_LIFETIME_SECONDS = 0.9;
constexpr double EFFECT_LIFETIME_SECONDS = 1.05;
constexpr int VFX_Z_INDEX = 70;

godot::Ref<godot::Texture2D> get_particle_texture() {
    static godot::Ref<godot::Texture2D> texture;
    if (texture.is_valid()) {
        return texture;
    }

    godot::Ref<godot::Image> image = godot::Image::create_empty(PARTICLE_TEXTURE_SIZE, PARTICLE_TEXTURE_SIZE, false, godot::Image::FORMAT_RGBA8);
    constexpr float radius = static_cast<float>(PARTICLE_TEXTURE_SIZE) * 0.5F;
    for (int pixel_y = 0; pixel_y < PARTICLE_TEXTURE_SIZE; ++pixel_y) {
        for (int pixel_x = 0; pixel_x < PARTICLE_TEXTURE_SIZE; ++pixel_x) {
            const godot::Vector2 offset(static_cast<float>(pixel_x) + 0.5F - radius, static_cast<float>(pixel_y) + 0.5F - radius);
            const float normalized_distance = offset.length() / radius;
            const float alpha = std::pow(std::clamp(1.0F - normalized_distance, 0.0F, 1.0F), 1.35F);
            image->set_pixel(pixel_x, pixel_y, godot::Color(1.0F, 1.0F, 1.0F, alpha));
        }
    }
    texture = godot::ImageTexture::create_from_image(image);
    return texture;
}

godot::Ref<godot::Gradient> make_color_ramp() {
    godot::Ref<godot::Gradient> gradient;
    gradient.instantiate();
    godot::PackedFloat32Array offsets;
    offsets.push_back(0.0F);
    offsets.push_back(0.62F);
    offsets.push_back(1.0F);
    godot::PackedColorArray colors;
    colors.push_back(godot::Color(1.0F, 0.72F, 0.12F, 0.95F));
    colors.push_back(godot::Color(1.0F, 0.94F, 0.52F, 0.78F));
    colors.push_back(godot::Color(1.0F, 1.0F, 0.82F, 0.0F));
    gradient->set_offsets(offsets);
    gradient->set_colors(colors);
    return gradient;
}

} // namespace

void FieldPromotionEffect::_bind_methods() {}

void FieldPromotionEffect::start() {
    auto *particles = memnew(godot::CPUParticles2D);
    particles->set_name("PromotionGoldBurst");
    particles->set_amount(PARTICLE_COUNT);
    particles->set_lifetime(PARTICLE_LIFETIME_SECONDS);
    particles->set_one_shot(true);
    particles->set_explosiveness_ratio(0.92F);
    particles->set_direction(godot::Vector2(0.0F, -1.0F));
    particles->set_spread(48.0F);
    particles->set_param_min(godot::CPUParticles2D::PARAM_INITIAL_LINEAR_VELOCITY, 60.0F);
    particles->set_param_max(godot::CPUParticles2D::PARAM_INITIAL_LINEAR_VELOCITY, 105.0F);
    particles->set_gravity(godot::Vector2(0.0F, 50.0F));
    particles->set_param_min(godot::CPUParticles2D::PARAM_SCALE, 0.336F);
    particles->set_param_max(godot::CPUParticles2D::PARAM_SCALE, 0.696F);
    particles->set_texture(get_particle_texture());
    particles->set_color_ramp(make_color_ramp());
    particles->set_z_index(VFX_Z_INDEX);
    add_child(particles);
    particles->set_emitting(true);
    set_process(true);
}

void FieldPromotionEffect::_process(double delta) {
    elapsed_seconds_ += delta;
    if (elapsed_seconds_ >= EFFECT_LIFETIME_SECONDS) {
        queue_free();
    }
}

void spawn_field_promotion_effect(godot::Node2D *parent, const godot::Vector2 &world_position) {
    if (parent == nullptr) {
        return;
    }
    auto *effect = memnew(FieldPromotionEffect);
    effect->set_name("FieldPromotionEffect");
    parent->add_child(effect);
    effect->set_global_position(world_position);
    effect->start();
}

} // namespace defn::vfx
