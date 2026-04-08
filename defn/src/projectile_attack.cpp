#include "projectile_attack.h"

#include "unit.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace defn {

namespace {

constexpr char PLAYBACK_ANIMATION[] = "play";

float linear_to_db(float linear) {
    const float clamped_linear = std::clamp(linear, 0.0001F, 1.0F);
    return 20.0F * std::log10(clamped_linear);
}

} // namespace

void ProjectileAttack::_bind_methods() {}

void ProjectileAttack::configure(const ProjectileAttackConfig &config, UnitSide shooter_side, const Color &flash_color, const Vector2 &start_global_position,
                                 const Vector2 &target_global_position, Unit *direct_target, int fallback_damage) {
    config_ = config;
    shooter_side_ = shooter_side;
    flash_color_ = flash_color;
    target_global_position_ = target_global_position;
    fallback_damage_ = fallback_damage;
    direct_target_id_ = direct_target != nullptr ? ObjectID(direct_target->get_instance_id()) : ObjectID();
    const real_t projectile_scale = MAX(config_.projectile_scale_multiplier, 0.01F);
    const real_t explosion_scale = MAX(config_.explosion_scale_multiplier, 0.01F);
    flight_scale_ = Vector2(projectile_scale, projectile_scale);
    explosion_scale_ = Vector2(explosion_scale, explosion_scale);
    exploding_ = false;
    explosion_animation_finished_ = false;
    explosion_sfx_finished_ = !config_.explosion_sfx.has_value() || config_.explosion_sfx->path.is_empty();
    travelled_distance_ = 0.0F;

    const Vector2 travel = target_global_position_ - start_global_position;
    total_travel_distance_ = travel.length();
    travel_direction_ = total_travel_distance_ > 0.0F ? travel / total_travel_distance_ : Vector2();

    ensure_sprite();
    set_global_position(start_global_position);
    set_scale(flight_scale_);
    start_flight_animation();
    set_process(true);

    if (total_travel_distance_ <= 0.0F || config_.speed_pixels_per_second <= 0.0F) {
        explode();
    }
}

void ProjectileAttack::_process(double delta) {
    if (exploding_) {
        return;
    }

    const real_t delta_seconds = static_cast<real_t>(delta);
    const real_t step_distance = config_.speed_pixels_per_second * delta_seconds;
    if (step_distance <= 0.0F) {
        explode();
        return;
    }

    const real_t remaining_distance = total_travel_distance_ - travelled_distance_;
    if (step_distance >= remaining_distance) {
        set_global_position(target_global_position_);
        travelled_distance_ = total_travel_distance_;
        explode();
        return;
    }

    travelled_distance_ += step_distance;
    set_global_position(get_global_position() + (travel_direction_ * step_distance));
}

Ref<SpriteFrames> ProjectileAttack::build_frames(const AnimConfig &animation) const {
    auto *loader = ResourceLoader::get_singleton();
    Ref<SpriteFrames> frames;
    frames.instantiate();
    frames->add_animation(PLAYBACK_ANIMATION);
    frames->set_animation_speed(PLAYBACK_ANIMATION, animation.speed);
    frames->set_animation_loop(PLAYBACK_ANIMATION, animation.loop);

    if (!animation.path_template.is_empty()) {
        for (int frame_index = 0; frame_index < animation.frame_count; ++frame_index) {
            const String path = vformat(animation.path_template, frame_index);
            Ref<Texture2D> texture = loader->load(path);
            if (texture.is_valid()) {
                frames->add_frame(PLAYBACK_ANIMATION, texture);
            }
        }
    }

    if (frames->has_animation("default")) {
        frames->remove_animation("default");
    }

    return frames;
}

void ProjectileAttack::ensure_sprite() {
    if (sprite_ != nullptr) {
        return;
    }

    sprite_ = memnew(AnimatedSprite2D);
    add_child(sprite_);
    sprite_->connect("animation_finished", callable_mp(this, &ProjectileAttack::on_animation_finished));
}

void ProjectileAttack::ensure_explosion_audio_player() {
    if (explosion_player_ != nullptr) {
        return;
    }

    explosion_player_ = memnew(AudioStreamPlayer2D);
    explosion_player_->set_name("ExplosionSfxPlayer");
    explosion_player_->connect("finished", callable_mp(this, &ProjectileAttack::on_explosion_sfx_finished));
    add_child(explosion_player_);
}

void ProjectileAttack::start_flight_animation() {
    if (sprite_ == nullptr) {
        return;
    }

    sprite_->set_sprite_frames(build_frames(config_.projectile_animation));
    sprite_->set_visible(true);
    sprite_->play(PLAYBACK_ANIMATION);
    sprite_->set_frame_and_progress(0, 0.0);
}

void ProjectileAttack::start_explosion_animation() {
    if (sprite_ == nullptr) {
        explosion_animation_finished_ = true;
        try_finish();
        return;
    }

    if (config_.explosion_animation.frame_count <= 0 || config_.explosion_animation.path_template.is_empty()) {
        explosion_animation_finished_ = true;
        try_finish();
        return;
    }

    sprite_->set_sprite_frames(build_frames(config_.explosion_animation));
    sprite_->set_visible(true);
    sprite_->play(PLAYBACK_ANIMATION);
    sprite_->set_frame_and_progress(0, 0.0);
}

void ProjectileAttack::play_explosion_sfx() {
    if (!config_.explosion_sfx.has_value() || config_.explosion_sfx->path.is_empty()) {
        explosion_sfx_finished_ = true;
        try_finish();
        return;
    }

    ensure_explosion_audio_player();
    auto *loader = ResourceLoader::get_singleton();
    Ref<AudioStream> stream = loader->load(config_.explosion_sfx->path);
    if (!stream.is_valid()) {
        explosion_sfx_finished_ = true;
        try_finish();
        return;
    }

    explosion_player_->set_stream(stream);
    explosion_player_->set_volume_db(linear_to_db(config_.explosion_sfx->volume_linear));
    const float variance = config_.explosion_sfx->pitch_variance;
    const float randomized_pitch =
        config_.explosion_sfx->pitch_scale + static_cast<float>(UtilityFunctions::randf_range(-variance, variance));
    explosion_player_->set_pitch_scale(randomized_pitch > 0.01F ? randomized_pitch : 0.01F);
    explosion_player_->play();
}

void ProjectileAttack::explode() {
    if (exploding_) {
        return;
    }

    exploding_ = true;
    set_process(false);
    set_global_position(target_global_position_);
    set_scale(explosion_scale_);
    apply_splash_damage();
    play_explosion_sfx();
    start_explosion_animation();
}

void ProjectileAttack::apply_splash_damage() {
    std::vector<Unit *> candidates;
    candidates.reserve(8);

    Unit *direct_target = resolve_direct_target();
    if (config_.include_direct_target && direct_target != nullptr && !direct_target->is_dead() && direct_target->get_side() != shooter_side_) {
        candidates.push_back(direct_target);
    }

    Node *parent = get_parent();
    if (parent != nullptr) {
        const Array children = parent->get_children();
        const real_t splash_radius_squared = config_.splash_radius * config_.splash_radius;
        for (const Variant &child_variant : children) {
            auto *unit = Object::cast_to<Unit>(child_variant.operator Object *());
            if (unit == nullptr || unit->is_dead() || unit->get_side() == shooter_side_) {
                continue;
            }

            if (std::ranges::find(candidates, unit) != candidates.end()) {
                continue;
            }

            if (unit->get_global_position().distance_squared_to(target_global_position_) > splash_radius_squared) {
                continue;
            }

            candidates.push_back(unit);
        }
    }

    if (candidates.empty()) {
        return;
    }

    const int affected_count = compute_affected_target_count(static_cast<int>(candidates.size()));
    for (int index = 0; index < affected_count; ++index) {
        Unit *victim = candidates[static_cast<size_t>(index)];
        if (victim == nullptr || victim->is_dead()) {
            continue;
        }

        const int damage = victim == direct_target ? resolve_impact_damage() : resolve_splash_damage();
        victim->take_damage(damage);
        victim->flash_damage(flash_color_);
    }
}

void ProjectileAttack::on_animation_finished() {
    if (exploding_) {
        explosion_animation_finished_ = true;
        try_finish();
    }
}

void ProjectileAttack::on_explosion_sfx_finished() {
    explosion_sfx_finished_ = true;
    try_finish();
}

Unit *ProjectileAttack::resolve_direct_target() const {
    if (direct_target_id_.is_null()) {
        return nullptr;
    }

    return Object::cast_to<Unit>(ObjectDB::get_instance(static_cast<uint64_t>(direct_target_id_)));
}

int ProjectileAttack::resolve_impact_damage() const { return config_.impact_damage.value_or(fallback_damage_); }

int ProjectileAttack::resolve_splash_damage() const { return config_.splash_damage.value_or(resolve_impact_damage()); }

int ProjectileAttack::compute_affected_target_count(int candidate_count) const {
    if (candidate_count <= 0) {
        return 0;
    }

    const real_t raw_count = static_cast<real_t>(candidate_count) * config_.affected_fraction;
    real_t rounded_count = raw_count;
    switch (config_.affected_target_rounding) {
    case SplashTargetRoundingMode::FLOOR:
        rounded_count = std::floor(raw_count);
        break;
    case SplashTargetRoundingMode::NEAREST:
        rounded_count = std::round(raw_count);
        break;
    case SplashTargetRoundingMode::CEIL:
        rounded_count = std::ceil(raw_count);
        break;
    }

    const int rounded_targets = static_cast<int>(rounded_count);
    return std::clamp(std::max(rounded_targets, config_.min_affected_targets), 1, candidate_count);
}

void ProjectileAttack::try_finish() {
    if (exploding_ && explosion_animation_finished_ && explosion_sfx_finished_) {
        queue_free();
    }
}

} // namespace defn