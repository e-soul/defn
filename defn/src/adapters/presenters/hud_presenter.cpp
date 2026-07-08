#include "hud_presenter.h"

#include <algorithm>

namespace defn {

namespace {

std::string format_level_text(int level_number, const std::string &level_name) {
    if (level_number > 0 && !level_name.empty()) {
        return "LEVEL " + std::to_string(level_number) + " - " + level_name;
    }
    if (level_number > 0) {
        return "LEVEL " + std::to_string(level_number);
    }
    return level_name;
}

} // namespace

HudModel HudPresenter::build(const HudPresentationInput &input) {
    HudModel model;
    model.energy_text = "\u26A1 Energy: " + std::to_string(input.energy);
    model.score_text = "Score: " + std::to_string(input.score);
    model.wave_text = "WAVE " + std::to_string(input.current_wave) + " / " + std::to_string(input.total_waves);
    model.level_text = format_level_text(input.level_number, input.level_name);
    model.level_visible = !model.level_text.empty();
    model.visible_hearts = std::max(0, input.hearts);

    model.deploy_cards.reserve(input.deploy_cards.size());
    for (const auto &card_input : input.deploy_cards) {
        DeployCardViewModel card = build_deploy_card_view_model(card_input);
        model.deploy_cards.push_back({
            .card = std::move(card),
            .enabled = input.energy >= card_input.cost,
        });
    }

    return model;
}

} // namespace defn