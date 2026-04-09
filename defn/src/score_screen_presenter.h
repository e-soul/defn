#ifndef SCORE_SCREEN_PRESENTER_H
#define SCORE_SCREEN_PRESENTER_H

#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace defn {

using namespace godot;

struct ScoreScreenActions {
    Callable on_next_level;
    Callable on_retry;
    Callable on_main_menu;
    Callable on_select_upgrade;
};

struct ScoreScreenView {
    ColorRect *overlay = nullptr;
    PanelContainer *panel = nullptr;
};

class ScoreScreenPresenter {
  public:
    ScoreScreenPresenter() = delete;

    static ScoreScreenView show(Node *parent, const Dictionary &stats, const ScoreScreenActions &actions);
};

} // namespace defn

#endif