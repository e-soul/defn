// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef DEPLOY_CARD_PRESENTER_H
#define DEPLOY_CARD_PRESENTER_H

#include "deploy_card_view_model.h"
#include "hud_layout.h"
#include "unit_definition.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/variant/callable.hpp>

namespace defn {

using namespace godot;

class DeployCardPresenter {
  public:
    DeployCardPresenter() = delete;

    static Button *create(const UnitConfig &config, const Callable &pressed_action);
    static Button *create(const DeployCardViewModel &view_model, const Callable &pressed_action);
    static HudSizing resolve_sizing(const UiViewportMetrics &viewport);
    static void apply_sizing(Button *button, const HudSizing &sizing);
};

} // namespace defn

#endif
