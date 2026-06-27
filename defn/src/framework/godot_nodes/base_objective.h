#ifndef BASE_OBJECTIVE_H
#define BASE_OBJECTIVE_H

#include "battle_entity.h"
#include "unit_data.h"

#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace defn {

using namespace godot;

class AnimationController;
class CombatComponent;
class DetectionComponent;
class HealthComponent;
class SoundController;

class BaseObjective : public BattleEntity {
    GDCLASS(BaseObjective, BattleEntity)

  public:
    BaseObjective();

    void configure(int max_hp, const Vector2 &position, const std::optional<UnitConfig> &visual_config = std::nullopt);
    void flash_damage(const Color &color) override;

    void _draw() override;
    void _process(double delta) override;

  protected:
    static void _bind_methods();

  private:
    void ensure_attack_components();
    void ensure_sprite();
    void ensure_health_component();
    void ensure_hitbox();
    bool set_sprite_animation(const String &animation_name);
    const AnimConfig *find_animation_config(const String &animation_name) const;
    static String resolve_animation_frame_path(const AnimConfig &animation, int frame_index);
    Vector2 get_local_anchor_position() const;
    void update_visual_state();
    void on_health_changed(int current_hp, int max_hp);
    void on_destroyed();

    AnimationController *animation_ = nullptr;
    CombatComponent *combat_ = nullptr;
    DetectionComponent *detection_ = nullptr;
    SoundController *sound_ = nullptr;
    Sprite2D *sprite_ = nullptr;
    Ref<Texture2D> sprite_texture_;
    std::optional<UnitConfig> visual_config_;
    Color flash_color_ = Color(1.0, 1.0, 1.0);
    real_t flash_time_remaining_ = 0.0F;
};

} // namespace defn

#endif