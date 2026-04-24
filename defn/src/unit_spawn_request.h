#ifndef UNIT_SPAWN_REQUEST_H
#define UNIT_SPAWN_REQUEST_H

#include "unit_data.h"

#include <godot_cpp/variant/vector2.hpp>

namespace defn {

using namespace godot;

struct UnitSpawnRequest {
    UnitConfig config;
    Vector2 position;
};

} // namespace defn

#endif