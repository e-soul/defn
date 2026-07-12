#ifndef UNIT_PROGRESSION_MAPPER_H
#define UNIT_PROGRESSION_MAPPER_H

#include "progression_rules.h"
#include "unit_definition.h"

namespace defn {

[[nodiscard]] ProgressionUnitStats to_progression_unit_stats(const UnitConfig &config);
[[nodiscard]] UnitConfig with_progression_unit_stats(UnitConfig config, const ProgressionUnitStats &stats);

} // namespace defn

#endif
