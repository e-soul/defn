#ifndef UNIT_FACTORY_H
#define UNIT_FACTORY_H

#include "unit_data.h"
#include <godot_cpp/variant/vector2.hpp>

namespace defn {

using namespace godot;

class Unit;

class UnitFactory {
  public:
    UnitFactory() = delete;

    static Unit *create(const UnitConfig &config, const Vector2 &position);
    static void initialize(Unit *unit);
};

} // namespace defn

#endif