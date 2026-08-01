// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UI_THEME_LOADER_H
#define UI_THEME_LOADER_H

#include "ui_theme_models.h"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>

namespace defn {

class UiThemeLoader {
  public:
    UiThemeLoader() = delete;

    static std::optional<UiThemeData> load(const godot::String &path);
    static UiThemeData load_from_data(const godot::Dictionary &data);
};

} // namespace defn

#endif
