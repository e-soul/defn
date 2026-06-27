#ifndef ATTACK_TARGET_RESOLVER_H
#define ATTACK_TARGET_RESOLVER_H

#include "attack_target.h"

#include <godot_cpp/core/object.hpp>

namespace defn {

using namespace godot;

AttackTarget *resolve_attack_target(Object *object);
AttackTarget *resolve_attack_target(const ObjectID &object_id);

} // namespace defn

#endif