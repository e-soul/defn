// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UNIT_H
#define UNIT_H

#include "battle_entity.h"
#include "field_promotion_runtime.h"
#include "unit_definition.h"
#include "unit_runtime_profile.h"
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/string_name.hpp>

namespace defn {

using namespace godot;

class HealthComponent;
class HealthBarWidget;
class AnimationController;
class CombatComponent;
class DetectionComponent;
class HitboxComponent;
class MovementComponent;
class SoundController;
class FieldPromotionView;
class UnitFactory;
class UnitControlComponent;

class Unit : public BattleEntity {
    GDCLASS(Unit, BattleEntity)

  public:
    Unit();
    ~Unit() override = default;

    void set_unit_config(const UnitConfig &cfg);
    void set_resolved_attack_ranges(real_t melee_range, real_t ranged_attack_range);
    void set_runtime_profile(const UnitRuntimeProfile &profile) { runtime_profile_ = profile; }
    void configure_field_promotion(const FieldPromotionRules &rules);

    void flash_damage(const godot::Color &color) override;

    int get_cost() const { return unit_config_.cost; }
    int get_bounty() const { return unit_config_.bounty; }
    const UnitConfig &get_unit_config() const { return unit_config_; }
    const UnitRuntimeProfile &get_runtime_profile() const { return runtime_profile_; }
    real_t get_attack_range() const { return attack_range; }
    real_t get_ranged_range() const { return ranged_range; }
    MovementComponent *get_movement_component() const override { return movement; }
    [[nodiscard]] int resolve_outgoing_damage(int base_damage) const { return field_promotion_.outgoing_damage(base_damage); }
    void record_effective_damage_dealt(int effective_damage);
    [[nodiscard]] bool is_field_promoted() const { return field_promotion_.is_promoted(); }
    [[nodiscard]] int get_promotion_damage_progress() const { return field_promotion_.get_effective_damage_dealt(); }
    [[nodiscard]] bool is_commandable() const;
    [[nodiscard]] bool contains_selection_point(const godot::Vector2 &world_position, real_t fallback_radius) const;
    [[nodiscard]] real_t get_selection_ground_offset_y(real_t fallback_world_offset) const;
    [[nodiscard]] bool request_reposition(real_t destination_x, float arrival_epsilon);
    void cancel_reposition_for_match_end();

    void freeze_for_match_result(const StringName &animation_name);

    void _ready() override;

  protected:
    static void _bind_methods();

  private:
    friend class UnitFactory;

    void on_died();
    void attach_health_component(HealthComponent *health_component) { set_health_component(health_component); }
    void attach_hitbox_component(HitboxComponent *hitbox_component) { set_hitbox_component(hitbox_component); }

    UnitConfig unit_config_;
    UnitRuntimeProfile runtime_profile_{};
    real_t attack_range = 128.0;
    real_t ranged_range = 384.0;
    bool runtime_initialized_ = false;

    HealthComponent *health = nullptr;
    HealthBarWidget *health_bar_widget = nullptr;
    AnimationController *animation = nullptr;
    CombatComponent *combat = nullptr;
    DetectionComponent *detection = nullptr;
    MovementComponent *movement = nullptr;
    SoundController *sound = nullptr;
    FieldPromotionView *field_promotion_view = nullptr;
    UnitControlComponent *unit_control_ = nullptr;
    FieldPromotionRuntime field_promotion_{};
};

} // namespace defn

#endif
