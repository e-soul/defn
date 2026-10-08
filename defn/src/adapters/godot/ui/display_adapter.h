// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef DISPLAY_ADAPTER_H
#define DISPLAY_ADAPTER_H
#include "responsive_layout.h"
#include <godot_cpp/classes/window.hpp>
namespace defn {
// Platform APIs end here. Layout receives only logical dimensions and capability values.
[[nodiscard]] DisplaySnapshot read_display(godot::Window *window);
void apply_display(godot::Window *window, const DisplaySnapshot &display);
} // namespace defn
#endif
