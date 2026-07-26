// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef VFX_BOUNTY_ENERGY_EFFECT_H
#define VFX_BOUNTY_ENERGY_EFFECT_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace defn::vfx {

void spawn_bounty_energy_effect(godot::Node2D *parent, godot::Vector2 world_position, int awarded_bounty);

} // namespace defn::vfx

#endif