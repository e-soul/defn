#ifndef DEPLOY_CARD_PRESENTER_H
#define DEPLOY_CARD_PRESENTER_H

#include "unit_data.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/variant/callable.hpp>

namespace defn {

using namespace godot;

class DeployCardPresenter {
  public:
    DeployCardPresenter() = delete;

    static Button *create(const UnitConfig &config, const Callable &pressed_action);
};

} // namespace defn

#endif