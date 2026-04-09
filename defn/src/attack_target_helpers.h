#ifndef ATTACK_TARGET_HELPERS_H
#define ATTACK_TARGET_HELPERS_H

#include "base_objective.h"
#include "unit.h"

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace defn::AttackTarget {

using namespace godot;

inline Node2D *resolve(Object *object) {
    if (auto *unit = Object::cast_to<Unit>(object)) {
        return unit;
    }

    if (auto *objective = Object::cast_to<BaseObjective>(object)) {
        return objective;
    }

    return nullptr;
}

inline bool is_dead(Object *object) {
    if (auto *unit = Object::cast_to<Unit>(object)) {
        return unit->is_dead();
    }

    if (auto *objective = Object::cast_to<BaseObjective>(object)) {
        return objective->is_dead();
    }

    return true;
}

inline UnitSide get_side(Object *object) {
    if (auto *unit = Object::cast_to<Unit>(object)) {
        return unit->get_side();
    }

    if (auto *objective = Object::cast_to<BaseObjective>(object)) {
        return objective->get_side();
    }

    return UnitSide::HOSTILE;
}

inline Vector2 get_global_position(Object *object) {
    if (auto *node = resolve(object)) {
        return node->get_global_position();
    }

    return {};
}

inline void take_damage(Object *object, int amount) {
    if (auto *unit = Object::cast_to<Unit>(object)) {
        unit->take_damage(amount);
        return;
    }

    if (auto *objective = Object::cast_to<BaseObjective>(object)) {
        objective->take_damage(amount);
    }
}

inline void flash_damage(Object *object, const Color &color) {
    if (auto *unit = Object::cast_to<Unit>(object)) {
        unit->flash_damage(color);
        return;
    }

    if (auto *objective = Object::cast_to<BaseObjective>(object)) {
        objective->flash_damage(color);
    }
}

} // namespace defn::AttackTarget

#endif