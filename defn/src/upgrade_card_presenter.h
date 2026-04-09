#ifndef UPGRADE_CARD_PRESENTER_H
#define UPGRADE_CARD_PRESENTER_H

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace defn {

using namespace godot;

class UpgradeCardPresenter {
  public:
    UpgradeCardPresenter() = delete;

    static Button *create(const Dictionary &upgrade_card, bool selected, bool disabled, const Callable &pressed_action);
};

} // namespace defn

#endif