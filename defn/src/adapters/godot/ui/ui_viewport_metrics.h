// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UI_VIEWPORT_METRICS_H
#define UI_VIEWPORT_METRICS_H

#include "hud_layout.h"

#include <godot_cpp/classes/viewport.hpp>

namespace defn {

[[nodiscard]] UiViewportMetrics measure_ui_viewport(godot::Viewport *viewport);

} // namespace defn

#endif
