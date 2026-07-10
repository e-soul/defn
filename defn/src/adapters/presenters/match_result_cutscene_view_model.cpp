#include "match_result_cutscene_view_model.h"

namespace defn {

MatchResultCutsceneModel MatchResultCutscenePresenter::build(bool victory) {
    if (victory) {
        return {
            .victory = true,
            .label = "AREA SECURED",
            .label_color = {.r = 0.2F, .g = 1.0F, .b = 0.3F, .a = 1.0F},
            .label_outline_color = {.r = 0.0F, .g = 0.23F, .b = 0.1F, .a = 1.0F},
            .primary_sfx_path = "res://assets/sfx/area_secured_sting.wav",
            .base_destroyed_sfx_path = "",
            .celebrant_side = MatchResultCelebrantSide::Friendly,
            .duration_seconds = 4.0,
            .reveal_delay_seconds = 0.0,
            .pre_reveal_animation_name = "",
            .reveal_animation_name = "happy",
            .shake_base = false,
        };
    }

    return {
        .victory = false,
        .label = "DEFEAT",
        .label_color = {.r = 1.0F, .g = 0.2F, .b = 0.2F, .a = 1.0F},
        .label_outline_color = {.r = 0.42F, .g = 0.0F, .b = 0.0F, .a = 1.0F},
        .primary_sfx_path = "res://assets/sfx/defeat_sting.wav",
        .base_destroyed_sfx_path = "res://assets/sfx/base_destroyed_collapse.wav",
        .celebrant_side = MatchResultCelebrantSide::Hostile,
        .duration_seconds = 4.0,
        .reveal_delay_seconds = 1.5,
        .pre_reveal_animation_name = "idle",
        .reveal_animation_name = "happy",
        .shake_base = true,
    };
}

} // namespace defn