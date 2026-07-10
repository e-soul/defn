#ifndef MATCH_RESULT_CUTSCENE_VIEW_MODEL_H
#define MATCH_RESULT_CUTSCENE_VIEW_MODEL_H

#include "content_values.h"

#include <string>

namespace defn {

enum class MatchResultCelebrantSide { Friendly, Hostile };

struct MatchResultCutsceneModel {
    bool victory = false;
    std::string label;
    Color label_color = {1.0F, 1.0F, 1.0F, 1.0F};
    Color label_outline_color = {0.0F, 0.0F, 0.0F, 1.0F};
    std::string primary_sfx_path;
    std::string base_destroyed_sfx_path;
    MatchResultCelebrantSide celebrant_side = MatchResultCelebrantSide::Friendly;
    double duration_seconds = 4.0;
    double reveal_delay_seconds = 0.0;
    std::string pre_reveal_animation_name;
    std::string reveal_animation_name = "happy";
    bool shake_base = false;
};

class MatchResultCutscenePresenter {
  public:
    MatchResultCutscenePresenter() = delete;

    [[nodiscard]] static MatchResultCutsceneModel build(bool victory);
};

} // namespace defn

#endif