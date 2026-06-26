#include "vfx/bounty_energy_effect.h"

#include <algorithm>

#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/polygon2d.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>

namespace defn::vfx {

using namespace godot;

namespace {

constexpr int MIN_MOTES = 2;
constexpr int MAX_MOTES = 8;
constexpr int VFX_Z_INDEX = 80;
constexpr real_t PRIMARY_BOLT_WIDTH = 22.0F;
constexpr real_t PRIMARY_BOLT_HEIGHT = 39.0F;
constexpr real_t PRIMARY_BOLT_LIFETIME_SECONDS = 0.55F;
constexpr real_t PRIMARY_BOLT_RISE_DISTANCE = 24.0F;
constexpr real_t PRIMARY_BOLT_START_SCALE = 0.62F;
constexpr real_t PRIMARY_BOLT_END_SCALE = 1.2F;
constexpr real_t MOTE_WIDTH = 8.0F;
constexpr real_t MOTE_HEIGHT = 15.0F;
constexpr real_t MOTE_START_SCALE = 1.08F;
constexpr real_t MOTE_END_SCALE = 0.72F;
constexpr real_t MOTE_LIFETIME_MIN_SECONDS = 0.58F;
constexpr real_t MOTE_LIFETIME_MAX_SECONDS = 0.82F;
constexpr real_t MOTE_VERTICAL_MIN = 34.0F;
constexpr real_t MOTE_VERTICAL_MAX = 58.0F;
constexpr real_t MOTE_HORIZONTAL_VARIANCE = 18.0F;
constexpr real_t MOTE_SPAWN_VARIANCE = 5.0F;

const Color PRIMARY_BOLT_COLOR = Color(1.0F, 0.98F, 0.30F, 1.0F);
const Color MOTE_COLOR = Color(0.92F, 1.0F, 0.42F, 1.0F);

int calculate_mote_count(int awarded_bounty) { return std::clamp(awarded_bounty, MIN_MOTES, MAX_MOTES); }

PackedVector2Array create_lightning_bolt_polygon(real_t width, real_t height) {
    PackedVector2Array polygon;
    polygon.append(Vector2(-0.12F * width, -0.50F * height));
    polygon.append(Vector2(0.38F * width, -0.50F * height));
    polygon.append(Vector2(0.12F * width, -0.06F * height));
    polygon.append(Vector2(0.40F * width, -0.06F * height));
    polygon.append(Vector2(-0.22F * width, 0.50F * height));
    polygon.append(Vector2(-0.04F * width, 0.08F * height));
    polygon.append(Vector2(-0.36F * width, 0.08F * height));
    return polygon;
}

void spawn_primary_bolt(Node2D *parent, const Vector2 &world_position) {
    auto *bolt = memnew(Polygon2D);
    bolt->set_name("BountyEnergyLightningBolt");
    bolt->set_polygon(create_lightning_bolt_polygon(PRIMARY_BOLT_WIDTH, PRIMARY_BOLT_HEIGHT));
    bolt->set_color(PRIMARY_BOLT_COLOR);
    bolt->set_antialiased(true);
    bolt->set_z_index(VFX_Z_INDEX + 2);
    parent->add_child(bolt);
    bolt->set_global_position(world_position + Vector2(0.0F, -8.0F));
    bolt->set_scale(Vector2(PRIMARY_BOLT_START_SCALE, PRIMARY_BOLT_START_SCALE));

    const Vector2 target_position = bolt->get_position() + Vector2(0.0F, -PRIMARY_BOLT_RISE_DISTANCE);
    Ref<Tween> tween = bolt->create_tween();
    tween->set_trans(Tween::TRANS_BACK);
    tween->set_ease(Tween::EASE_OUT);
    tween->tween_property(bolt, NodePath("position"), target_position, PRIMARY_BOLT_LIFETIME_SECONDS);
    tween->parallel()->tween_property(bolt, NodePath("scale"), Vector2(PRIMARY_BOLT_END_SCALE, PRIMARY_BOLT_END_SCALE),
                                      PRIMARY_BOLT_LIFETIME_SECONDS);
    tween->parallel()->tween_property(bolt, NodePath("modulate"), Color(1.0F, 1.0F, 1.0F, 0.0F), PRIMARY_BOLT_LIFETIME_SECONDS);
    Node *bolt_node = bolt;
    tween->chain()->tween_callback(callable_mp(bolt_node, &Node::queue_free));
}

void spawn_mote(Node2D *parent, const Vector2 &world_position, const Ref<RandomNumberGenerator> &rng) {
    auto *mote = memnew(Polygon2D);
    mote->set_name("BountyEnergyLightningSpark");
    mote->set_polygon(create_lightning_bolt_polygon(MOTE_WIDTH, MOTE_HEIGHT));
    mote->set_color(MOTE_COLOR);
    mote->set_antialiased(true);
    mote->set_scale(Vector2(MOTE_START_SCALE, MOTE_START_SCALE));
    mote->set_z_index(VFX_Z_INDEX + 1);
    parent->add_child(mote);

    const Vector2 spawn_offset(rng->randf_range(-MOTE_SPAWN_VARIANCE, MOTE_SPAWN_VARIANCE),
                               rng->randf_range(-MOTE_SPAWN_VARIANCE, MOTE_SPAWN_VARIANCE));
    mote->set_global_position(world_position + spawn_offset);

    const Vector2 target_position = mote->get_position() +
                                    Vector2(rng->randf_range(-MOTE_HORIZONTAL_VARIANCE, MOTE_HORIZONTAL_VARIANCE),
                                            -rng->randf_range(MOTE_VERTICAL_MIN, MOTE_VERTICAL_MAX));
    const double lifetime = rng->randf_range(MOTE_LIFETIME_MIN_SECONDS, MOTE_LIFETIME_MAX_SECONDS);

    Ref<Tween> tween = mote->create_tween();
    tween->set_trans(Tween::TRANS_SINE);
    tween->set_ease(Tween::EASE_OUT);
    tween->tween_property(mote, NodePath("position"), target_position, lifetime);
    tween->parallel()->tween_property(mote, NodePath("modulate"), Color(1.0F, 1.0F, 1.0F, 0.0F), lifetime);
    tween->parallel()->tween_property(mote, NodePath("scale"), Vector2(MOTE_END_SCALE, MOTE_END_SCALE), lifetime);
    Node *mote_node = mote;
    tween->chain()->tween_callback(callable_mp(mote_node, &Node::queue_free));
}

} // namespace

void spawn_bounty_energy_effect(Node2D *parent, Vector2 world_position, int awarded_bounty) {
    if (parent == nullptr || awarded_bounty <= 0) {
        return;
    }

    spawn_primary_bolt(parent, world_position);

    Ref<RandomNumberGenerator> rng;
    rng.instantiate();
    rng->randomize();

    const int mote_count = calculate_mote_count(awarded_bounty);
    for (int index = 0; index < mote_count; ++index) {
        spawn_mote(parent, world_position, rng);
    }
}

} // namespace defn::vfx