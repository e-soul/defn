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

    static Button *create(const UpgradeCardViewModel &upgrade_card, bool selected, bool disabled, const Callable &pressed_action);
};

} // namespace defn

#endif