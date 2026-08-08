// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UNIT_CONTROL_CONFIG_LOADER_H
#define UNIT_CONTROL_CONFIG_LOADER_H

#include "unit_control_config.h"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>

namespace defn {

class UnitControlConfigLoader {
  public:
    UnitControlConfigLoader() = delete;

    static std::optional<UnitControlConfig> load(const godot::String &path);
    static UnitControlConfig load_from_data(const godot::Dictionary &data);
};

} // namespace defn

#endif
