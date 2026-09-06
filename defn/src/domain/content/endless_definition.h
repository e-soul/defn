// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef ENDLESS_DEFINITION_H
#define ENDLESS_DEFINITION_H

#include "endless_wave_generator.h"
#include "level_definition.h"

namespace defn {

// Everything `data/endless.json` carries: the ground the run is fought on, and the schedule that generates it. The
// level authors no waves -- the generator supplies all of them.
struct EndlessDefinition {
    LevelDefinition level;
    EndlessSchedule schedule;
};

} // namespace defn

#endif
