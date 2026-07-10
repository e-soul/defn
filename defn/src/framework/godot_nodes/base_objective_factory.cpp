#include "base_objective_factory.h"

#include "base_objective.h"

#include <godot_cpp/core/memory.hpp>

namespace defn {

BaseObjective *BaseObjectiveFactory::create(int max_hp, const godot::Vector2 &position, const std::optional<UnitConfig> &visual_config) {
    auto *objective = memnew(BaseObjective);
    objective->set_name("BaseObjective");
    objective->configure(max_hp, position, visual_config);
    return objective;
}

} // namespace defn