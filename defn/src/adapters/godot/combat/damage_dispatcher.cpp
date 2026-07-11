#include "damage_dispatcher.h"

#include "attack_target.h"
#include "unit.h"

#include <godot_cpp/core/object.hpp>

namespace defn {

int DamageDispatcher::apply(godot::ObjectID source_id, AttackTarget *target, int base_damage) {
    if (target == nullptr) {
        return 0;
    }

    Unit *source = source_id.is_valid() ? godot::Object::cast_to<Unit>(godot::ObjectDB::get_instance(source_id)) : nullptr;
    const bool live_friendly_source = source != nullptr && !source->is_dead() && source->get_side() == UnitSide::FRIENDLY;
    const int resolved_damage = live_friendly_source ? source->resolve_outgoing_damage(base_damage) : base_damage;
    const int effective_damage = target->take_damage(resolved_damage);
    if (live_friendly_source && source->get_side() != target->get_side()) {
        source->record_effective_damage_dealt(effective_damage);
    }
    return effective_damage;
}

} // namespace defn
