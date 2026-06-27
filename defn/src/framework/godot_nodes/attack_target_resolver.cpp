#include "attack_target_resolver.h"

#include "battle_entity.h"

#include <godot_cpp/core/object.hpp>

namespace defn {

AttackTarget *resolve_attack_target(Object *object) {
    if (auto *entity = Object::cast_to<BattleEntity>(object)) {
        return entity;
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