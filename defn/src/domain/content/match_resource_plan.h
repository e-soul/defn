// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef MATCH_RESOURCE_PLAN_H
#define MATCH_RESOURCE_PLAN_H

#include "endless_wave_generator.h"
#include "level_definition.h"
#include "unit_definition.h"
#include <span>
#include <string>
#include <vector>

namespace defn {
// The entire match's roster, including later waves and actual endless budget allocations, without consuming RNG.
[[nodiscard]] std::vector<std::string> match_unit_ids(const LevelDefinition &level, std::span<const std::string> friendly_ids,
                                                      const EndlessSchedule *endless = nullptr);
[[nodiscard]] std::string animation_frame_path(const std::string &path_template, int frame);
[[nodiscard]] std::vector<std::string> match_resource_paths(const LevelDefinition &level, std::span<const UnitConfig> units,
                                                            std::span<const std::string> presentation_paths = {});
} // namespace defn
#endif
