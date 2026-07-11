#ifndef DAMAGE_DISPATCHER_H
#define DAMAGE_DISPATCHER_H

#include <godot_cpp/core/object_id.hpp>

namespace defn {

class AttackTarget;

class DamageDispatcher {
  public:
    DamageDispatcher() = delete;

    [[nodiscard]] static int apply(godot::ObjectID source_id, AttackTarget *target, int base_damage);
};

} // namespace defn

#endif
