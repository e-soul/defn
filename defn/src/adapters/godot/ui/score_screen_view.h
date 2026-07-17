#ifndef SCORE_SCREEN_VIEW_H
#define SCORE_SCREEN_VIEW_H

#include "score_screen_models.h"

#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/variant/callable.hpp>

namespace defn {

using namespace godot;

class UiSfxPlayer;

struct ScoreScreenActions {
    Callable on_next_level;
    Callable on_retry;
    Callable on_main_menu;
    Callable on_select_upgrade;
};

struct ScoreScreenViewNodes {
    ColorRect *overlay = nullptr;
    PanelContainer *panel = nullptr;
};

class ScoreScreenView {
  public:
    ScoreScreenView() = delete;

    static ScoreScreenViewNodes show(Node *parent, const ScoreScreenModel &model, const ScoreScreenActions &actions, UiSfxPlayer *ui_sfx_player = nullptr);
};

} // namespace defn

#endif
