#ifndef BASE_OBJECTIVE_FACTORY_H
#define BASE_OBJECTIVE_FACTORY_H

#include "unit_definition.h"

#include <godot_cpp/variant/vector2.hpp>

#include <optional>

namespace defn {

using namespace godot;

class BaseObjective;

class BaseObjectiveFactory {
  public:
    BaseObjectiveFactory() = delete;

    static BaseObjective *create(int max_hp, const godot::Vector2 &position, const std::optional<UnitConfig> &visual_config = std::nullopt);
};

} // namespace defn

#endif