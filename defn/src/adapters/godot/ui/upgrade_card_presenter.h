// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef UPGRADE_CARD_PRESENTER_H
#define UPGRADE_CARD_PRESENTER_H

#include "score_screen_models.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/variant/callable.hpp>

namespace defn {

using namespace godot;

class UpgradeCardPresenter {
  public:
    UpgradeCardPresenter() = delete;

    /// `interactive` is false for a card that is only on display, such as an upgrade the player already owns.
    static Button *create(const UpgradeCardViewModel &upgrade_card, bool selected, bool disabled, const Callable &pressed_action, bool interactive = true);
};

} // namespace defn

#endif