#ifndef UNIT_FACTORY_H
#define UNIT_FACTORY_H

#include "match_outputs.h"
#include "unit_definition.h"
#include "unit_runtime_config_resolver.h"
#include "unit_runtime_profile.h"
#include <godot_cpp/variant/vector2.hpp>

namespace defn {

using namespace godot;

class Unit;

class UnitFactory {
  public:
    UnitFactory() = delete;

    static Unit *create(const UnitConfig &config, const Vector2 &position, const UnitRuntimeProfile &profile, const ResolvedUnitRuntimeConfig &resolved_config);
    static Unit *materialize(const SpawnUnitIntent &intent, const UnitConfig &config);
    static void initialize(Unit *unit);
};

} // namespace defn

#endif