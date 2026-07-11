#include "field_promotion_effect.h"

#include <godot_cpp/classes/cpu_particles2d.hpp>
#include <godot_cpp/classes/gradient.hpp>

namespace defn::vfx {

namespace {

constexpr int PARTICLE_COUNT = 10;
constexpr double PARTICLE_LIFETIME_SECONDS = 0.65;
constexpr double EFFECT_LIFETIME_SECONDS = 0.8;
constexpr int VFX_Z_INDEX = 70;

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
    particles->set_explosiveness_ratio(0.95F);
    particles->set_direction(godot::Vector2(0.0F, -1.0F));
    particles->set_spread(28.0F);
    particles->set_param_min(godot::CPUParticles2D::PARAM_INITIAL_LINEAR_VELOCITY, 28.0F);
    particles->set_param_max(godot::CPUParticles2D::PARAM_INITIAL_LINEAR_VELOCITY, 52.0F);
    particles->set_gravity(godot::Vector2(0.0F, 34.0F));
    particles->set_param_min(godot::CPUParticles2D::PARAM_SCALE, 1.5F);
    particles->set_param_max(godot::CPUParticles2D::PARAM_SCALE, 3.5F);
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
