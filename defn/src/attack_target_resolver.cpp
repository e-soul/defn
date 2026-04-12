#include "attack_target_resolver.h"

#include "base_objective.h"
#include "unit.h"

#include <godot_cpp/core/object.hpp>

namespace defn {

AttackTarget *resolve_attack_target(Object *object) {
    if (auto *unit = Object::cast_to<Unit>(object)) {
        return unit;
    }

    if (auto *objective = Object::cast_to<BaseObjective>(object)) {
        return objective;
    }

    return nullptr;
}

AttackTarget *resolve_attack_target(const ObjectID &object_id) {
    if (object_id.is_null()) {
        return nullptr;
    }

    return resolve_attack_target(ObjectDB::get_instance(static_cast<uint64_t>(object_id)));
}

} // namespace defn