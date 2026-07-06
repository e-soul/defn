#include "base_objective.h"

#include "animation_controller.h"
#include "collision_layers.h"
#include "combat_component.h"
#include "detection_component.h"
#include "health_component.h"
#include "hitbox_component.h"
#include "sound_controller.h"

#include <algorithm>

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

constexpr real_t OBJECTIVE_RADIUS = 96.0F;
constexpr real_t OBJECTIVE_CORE_RADIUS = 32.0F;
constexpr real_t OBJECTIVE_OUTLINE_RADIUS = 108.0F;
constexpr real_t DAMAGE_FLASH_DURATION_SECONDS = 0.12F;

constexpr auto OBJECTIVE_IDLE_ANIMATION = "idle";
constexpr auto OBJECTIVE_DEATH_ANIMATION = "death";

CombatColor to_combat_color(const Color &color) {
    return {
        .r = static_cast<float>(color.r),
        .g = static_cast<float>(color.g),
        .b = static_cast<float>(color.b),
        .a = static_cast<float>(color.a),
    };
}

const Color OBJECTIVE_FILL_COLOR = Color(0.82, 0.11, 0.08);
const Color OBJECTIVE_OUTLINE_COLOR = Color(0.32, 0.03, 0.03);
const Color OBJECTIVE_CORE_COLOR = Color(1.0, 0.38, 0.36, 0.9);

bool objective_can_attack(const std::optional<UnitConfig> &config) {
    return config.has_value() && (config->melee_damage > 0 || config->ranged_damage > 0 || config->projectile_attack.has_value());
}

CombatComponent::Config make_combat_config(const UnitConfig &config) {
    CombatComponent::Config combat_config;
    const bool has_melee_attack = config.melee_damage > 0;
    const bool has_ranged_attack = config.ranged_damage > 0 || config.projectile_attack.has_value();
    combat_config.side = config.side;
    combat_config.melee_damage = config.melee_damage;
    combat_config.melee_attack_period_seconds = config.melee_attack_period_seconds;
    combat_config.ranged_damage = config.ranged_damage;
    combat_config.ranged_attack_period_seconds = config.ranged_attack_period_seconds;
    combat_config.attack_range = has_melee_attack ? config.melee_attack_range : -1.0F;
    combat_config.ranged_range = has_ranged_attack ? config.ranged_attack_range : -1.0F;
    combat_config.melee_flash_color = to_combat_color(config.melee_flash_color);
    combat_config.ranged_flash_color = to_combat_color(config.ranged_flash_color);
    combat_config.projectile_attack = config.projectile_attack;
    return combat_config;
}

} // namespace

BaseObjective::BaseObjective() { set_process(false); }

void BaseObjective::_bind_methods() {
    ADD_SIGNAL(MethodInfo("durability_changed", PropertyInfo(Variant::INT, "current_hp"), PropertyInfo(Variant::INT, "max_hp")));
    ADD_SIGNAL(MethodInfo("objective_destroyed"));
}

void BaseObjective::configure(int max_hp, const Vector2 &position, const std::optional<UnitConfig> &visual_config) {
    visual_config_ = visual_config;
    if (visual_config_.has_value()) {
        set_side(visual_config_->side);
    }
    ensure_sprite();
    Node2D *target_anchor = ensure_target_anchor();
    ensure_health_component();
    ensure_hitbox();
    ensure_attack_components();

    const Vector2 local_anchor = get_local_anchor_position();
    set_position(position - local_anchor);
    target_anchor->set_position(local_anchor);
    if (HitboxComponent *hitbox = get_hitbox_component()) {
        hitbox->set_local_position(local_anchor);
    }
    if (detection_ != nullptr) {
        detection_->set_local_position(local_anchor);
    }

    if (HealthComponent *health = get_health_component()) {
        health->configure(max_hp);
    }
    update_visual_state();
    queue_redraw();
}

void BaseObjective::ensure_attack_components() {
    if (!objective_can_attack(visual_config_)) {
        return;
    }

    const UnitConfig &config = *visual_config_;
    const auto detection_channels = get_detection_channels(get_side());

    if (animation_ == nullptr) {
        animation_ = memnew(AnimationController);
        animation_->set_name("AnimationController");
        add_child(animation_);
        animation_->configure(this, config, false);
    }

    if (sound_ == nullptr) {
        sound_ = memnew(SoundController);
        sound_->set_name("SoundController");
        add_child(sound_);
        sound_->configure(this, config);
        if (animation_ != nullptr) {
            animation_->connect("shoot_effect_triggered", callable_mp(sound_, &SoundController::play_shoot_sfx));
        }
    }

    if (detection_ == nullptr) {
        detection_ = memnew(DetectionComponent);
        detection_->set_name("DetectionComponent");
        add_child(detection_);
        detection_->configure(this, detection_channels.sensor_mask, config.ranged_attack_range, get_scale().x);
    }

    if (combat_ == nullptr) {
        combat_ = memnew(CombatComponent);
        combat_->set_name("CombatComponent");
        add_child(combat_);
        combat_->configure(this, get_health_component(), animation_, detection_->get_detection_area(), make_combat_config(config));
    }
}

void BaseObjective::flash_damage(const Color &color) {
    if (is_dead()) {
        return;
    }

    flash_color_ = color;
    flash_time_remaining_ = DAMAGE_FLASH_DURATION_SECONDS;
    set_process(true);
    update_visual_state();
}

void BaseObjective::_draw() {
    if (sprite_ != nullptr && sprite_texture_.is_valid()) {
        return;
    }

    const bool flashing = flash_time_remaining_ > 0.0F;
    const Color fill_color = flashing ? OBJECTIVE_FILL_COLOR.lerp(flash_color_, 0.55F) : OBJECTIVE_FILL_COLOR;
    const Vector2 draw_origin = get_local_anchor_position();

    draw_circle(draw_origin, OBJECTIVE_OUTLINE_RADIUS, OBJECTIVE_OUTLINE_COLOR);
    draw_circle(draw_origin, OBJECTIVE_RADIUS, fill_color);
    draw_circle(draw_origin, OBJECTIVE_CORE_RADIUS, OBJECTIVE_CORE_COLOR);
}

void BaseObjective::_process(double delta) {
    if (flash_time_remaining_ <= 0.0F) {
        update_visual_state();
        set_process(false);
        return;
    }

    flash_time_remaining_ = std::max(flash_time_remaining_ - static_cast<real_t>(delta), 0.0F);
    update_visual_state();
    if (flash_time_remaining_ <= 0.0F) {
        set_process(false);
    }
}

void BaseObjective::ensure_sprite() {
    if (sprite_ != nullptr) {
        return;
    }

    if (!set_sprite_animation(OBJECTIVE_IDLE_ANIMATION)) {
        return;
    }

    sprite_->set_position(Vector2());
}

bool BaseObjective::set_sprite_animation(const String &animation_name) {
    const AnimConfig *animation = find_animation_config(animation_name);
    if (animation == nullptr) {
        return false;
    }

    auto *loader = ResourceLoader::get_singleton();
    if (loader == nullptr) {
        return false;
    }

    const String texture_path = resolve_animation_frame_path(*animation, 0);
    if (texture_path.is_empty()) {
        return false;
    }

    sprite_texture_ = loader->load(texture_path);
    if (!sprite_texture_.is_valid()) {
        UtilityFunctions::printerr("BaseObjective: Failed to load sprite: ", texture_path);
        return false;
    }

    if (sprite_ == nullptr) {
        sprite_ = memnew(Sprite2D);
        sprite_->set_name("TowerSprite");
        sprite_->set_centered(false);
        add_child(sprite_);
    }

    sprite_->set_texture(sprite_texture_);
    sprite_->set_flip_h(visual_config_.has_value() && visual_config_->sprite_flip_h);
    const real_t sprite_scale = visual_config_.has_value() ? visual_config_->scale : 1.0F;
    sprite_->set_scale(Vector2(sprite_scale, sprite_scale));

    return true;
}

const AnimConfig *BaseObjective::find_animation_config(const String &animation_name) const {
    if (!visual_config_.has_value()) {
        return nullptr;
    }

    for (const auto &[candidate_name, animation] : visual_config_->animations) {
        if (candidate_name == animation_name) {
            return &animation;
        }
    }

    return nullptr;
}

String BaseObjective::resolve_animation_frame_path(const AnimConfig &animation, int frame_index) {
    if (animation.path_template.is_empty()) {
        return {};
    }

    if (animation.path_template.contains("%")) {
        return vformat(animation.path_template, frame_index);
    }

    return animation.path_template;
}

void BaseObjective::ensure_health_component() {
    if (get_health_component() != nullptr) {
        return;
    }

    auto *health = memnew(HealthComponent);
    health->set_name("HealthComponent");
    add_child(health);
    set_health_component(health);
    health->connect("health_changed", callable_mp(this, &BaseObjective::on_health_changed));
    health->connect("died", callable_mp(this, &BaseObjective::on_destroyed));
}

void BaseObjective::ensure_hitbox() {
    if (get_hitbox_component() != nullptr) {
        return;
    }

    auto *hitbox = memnew(HitboxComponent);
    hitbox->set_name("HitboxComponent");
    add_child(hitbox);
    hitbox->configure(this, CollisionLayers::FRIENDLY_HITBOX, OBJECTIVE_RADIUS);
    set_hitbox_component(hitbox);
}

Vector2 BaseObjective::get_local_anchor_position() const {
    if (sprite_texture_.is_valid()) {
        const Vector2 texture_size = sprite_texture_->get_size();
        if (texture_size.x > 0.0F && texture_size.y > 0.0F) {
            const real_t scale_factor = visual_config_.has_value() ? visual_config_->scale : 1.0F;
            const Vector2 display_size = texture_size * scale_factor;
            return {
                display_size.x * 0.5F,
                std::max(display_size.y - (OBJECTIVE_RADIUS * 0.5F), OBJECTIVE_RADIUS),
            };
        }
    }

    return {OBJECTIVE_RADIUS, OBJECTIVE_RADIUS};
}

void BaseObjective::update_visual_state() {
    if (sprite_ != nullptr) {
        const bool flashing = flash_time_remaining_ > 0.0F;
        const Color tint = flashing ? Color(1.0, 1.0, 1.0).lerp(flash_color_, 0.35F) : Color(1.0, 1.0, 1.0);
        sprite_->set_modulate(tint);
        return;
    }

    queue_redraw();
}

void BaseObjective::on_health_changed(int current_hp, int max_hp) { emit_signal("durability_changed", current_hp, max_hp); }

void BaseObjective::on_destroyed() {
    flash_time_remaining_ = 0.0F;
    set_process(false);

    if (animation_ != nullptr) {
        animation_->hide_muzzle_flash();
    }

    if (HitboxComponent *hitbox = get_hitbox_component()) {
        hitbox->disable();
    }

    if (set_sprite_animation(OBJECTIVE_DEATH_ANIMATION) && sprite_ != nullptr) {
        const Node2D *target_anchor = get_target_anchor();
        const Vector2 anchor_position = target_anchor != nullptr ? target_anchor->get_position() : Vector2();
        sprite_->set_position(anchor_position - get_local_anchor_position());
        sprite_->set_modulate(Color(1.0, 1.0, 1.0));
    }

    emit_signal("objective_destroyed");
}

} // namespace defn