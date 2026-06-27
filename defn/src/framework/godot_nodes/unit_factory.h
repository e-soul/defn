#ifndef UNIT_FACTORY_H
#define UNIT_FACTORY_H

#include "unit_data.h"
#include "unit_runtime_profile.h"
#include "unit_spawn_request.h"
#include <godot_cpp/variant/vector2.hpp>

namespace defn {

using namespace godot;

class Unit;

class UnitFactory {
  public:
    UnitFactory() = delete;

    static Unit *create(const UnitConfig &config, const Vector2 &position);
    static Unit *create(const UnitConfig &config, const Vector2 &position, const UnitRuntimeProfile &profile);
    static Unit *materialize(const UnitSpawnRequest &request);
    static Unit *materialize(const UnitSpawnRequest &request, const UnitRuntimeProfile &profile);
    static void initialize(Unit *unit);
};

} // namespace defn

#endif