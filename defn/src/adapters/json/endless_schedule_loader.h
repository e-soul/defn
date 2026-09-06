// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef ENDLESS_SCHEDULE_LOADER_H
#define ENDLESS_SCHEDULE_LOADER_H

#include "endless_definition.h"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>

namespace defn {

using namespace godot;

class EndlessScheduleLoader {
  public:
    EndlessScheduleLoader() = delete;

    static std::optional<EndlessDefinition> load(const String &path);
    static std::optional<EndlessDefinition> load_from_data(const Dictionary &data);
};

} // namespace defn

#endif
