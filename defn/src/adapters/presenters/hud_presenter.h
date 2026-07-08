#ifndef HUD_PRESENTER_H
#define HUD_PRESENTER_H

#include "deploy_card_view_model.h"

#include <string>
#include <vector>

namespace defn {

struct HudPresentationInput {
    int energy = 0;
    int current_wave = 0;
    int total_waves = 0;
    int hearts = 0;
    int score = 0;
    int level_number = 0;
    std::string level_name;
    std::vector<DeployCardPresentationInput> deploy_cards;
};

struct HudDeployCardModel {
    DeployCardViewModel card;
    bool enabled = false;
};

struct HudModel {
    std::string energy_text;
    std::string score_text;
    std::string wave_text;
    std::string level_text;
    bool level_visible = false;
    int visible_hearts = 0;
    std::vector<HudDeployCardModel> deploy_cards;
};

class HudPresenter {
  public:
    HudPresenter() = delete;

    [[nodiscard]] static HudModel build(const HudPresentationInput &input);
};

} // namespace defn

#endif