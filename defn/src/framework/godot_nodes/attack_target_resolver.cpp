#include "attack_target_resolver.h"

#include "battle_entity.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/object.hpp>

namespace defn {

AttackTarget *resolve_attack_target(Object *object) {
    Object *current = object;
    while (current != nullptr) {
        if (auto *entity = Object::cast_to<BattleEntity>(current)) {
            return entity;
        }

        auto *node = Object::cast_to<Node>(current);
        if (node == nullptr) {
            break;
        }
        current = node->get_parent();
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