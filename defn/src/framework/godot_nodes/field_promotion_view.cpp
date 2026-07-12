#include "field_promotion_view.h"

#include "animation_controller.h"
#include "field_promotion_effect.h"
#include "health_bar_widget.h"

#include <algorithm>
#include <cmath>

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player2d.hpp>
#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/node_path.hpp>

namespace defn {

namespace {

constexpr auto PROMOTION_SFX_PATH = "res://assets/sfx/field_promotion_kalimba.wav";
constexpr int STAR_FONT_SIZE = 68;
constexpr int STAR_OUTLINE_SIZE = 9;
constexpr float STAR_BAR_GAP = 4.0F;
constexpr float SFX_VOLUME_LINEAR = 0.55F;
const godot::Color STAR_COLOR(1.0F, 0.76F, 0.12F, 1.0F);
const godot::Color OUTLINE_COLOR(0.10F, 0.07F, 0.02F, 0.95F);

float linear_to_db(float linear) { return 20.0F * std::log10(std::clamp(linear, 0.0001F, 1.0F)); }

} // namespace

void FieldPromotionView::_bind_methods() {}

void FieldPromotionView::configure(AnimationController *animation, HealthBarWidget *health_bar) {
    animation_ = animation;
    health_bar_ = health_bar;
}

void FieldPromotionView::show_promotion() {
    if (shown_) {
        return;
    }
    shown_ = true;
    create_insignia();
    play_pulse();

    auto *unit = godot::Object::cast_to<godot::Node2D>(get_parent());
    auto *effect_parent = unit != nullptr ? godot::Object::cast_to<godot::Node2D>(unit->get_parent()) : nullptr;
    if (unit == nullptr || effect_parent == nullptr) {
        return;
    }
    const godot::Rect2 bounds = animation_ != nullptr ? animation_->get_sprite_local_bounds() : godot::Rect2();
    const godot::Vector2 torso_position = unit->to_global(godot::Vector2(0.0F, bounds.position.y + (bounds.size.y * 0.38F)));
    vfx::spawn_field_promotion_effect(effect_parent, torso_position);
    play_sound(effect_parent, torso_position);
}

void FieldPromotionView::create_insignia() {
    insignia_ = memnew(godot::Label);
    insignia_->set_name("FieldPromotionInsignia");
    insignia_->set_text(String::chr(0x2605));
    insignia_->set_mouse_filter(godot::Control::MOUSE_FILTER_IGNORE);
    insignia_->add_theme_font_size_override("font_size", STAR_FONT_SIZE);
    insignia_->add_theme_constant_override("outline_size", STAR_OUTLINE_SIZE);
    insignia_->add_theme_color_override("font_color", STAR_COLOR);
    insignia_->add_theme_color_override("font_outline_color", OUTLINE_COLOR);
    insignia_->set_z_index(75);
    add_child(insignia_);

    const godot::Rect2 bar_rect = health_bar_ != nullptr ? health_bar_->get_bar_rect() : godot::Rect2(-85.0F, -128.0F, 170.0F, 10.0F);
    const godot::Vector2 insignia_size = insignia_->get_combined_minimum_size();
    insignia_->set_position({bar_rect.get_center().x - (insignia_size.x * 0.5F), bar_rect.position.y - insignia_size.y - STAR_BAR_GAP});
    insignia_->set_pivot_offset(insignia_size * 0.5F);
}

void FieldPromotionView::play_pulse() {
    if (insignia_ == nullptr) {
        return;
    }
    insignia_->set_scale(godot::Vector2(1.55F, 1.55F));
    insignia_->set_modulate(godot::Color(1.0F, 0.88F, 0.35F, 0.45F));
    godot::Ref<godot::Tween> tween = create_tween();
    tween->set_trans(godot::Tween::TRANS_BACK);
    tween->set_ease(godot::Tween::EASE_OUT);
    tween->tween_property(insignia_, godot::NodePath("scale"), godot::Vector2(1.0F, 1.0F), 0.22);
    tween->parallel()->tween_property(insignia_, godot::NodePath("modulate"), godot::Color(1.0F, 1.0F, 1.0F, 1.0F), 0.22);
}

void FieldPromotionView::play_sound(godot::Node2D *effect_parent, const godot::Vector2 &world_position) {
    auto *loader = godot::ResourceLoader::get_singleton();
    godot::Ref<godot::AudioStream> stream = loader->load(PROMOTION_SFX_PATH);
    if (!stream.is_valid()) {
        return;
    }
    auto *player = memnew(godot::AudioStreamPlayer2D);
    player->set_name("FieldPromotionSfxPlayer");
    effect_parent->add_child(player);
    player->set_global_position(world_position);
    player->set_stream(stream);
    player->set_volume_db(linear_to_db(SFX_VOLUME_LINEAR));
    godot::Node *player_node = player;
    player->connect("finished", callable_mp(player_node, &godot::Node::queue_free));
    player->play();
}

} // namespace defn
