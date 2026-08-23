// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

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
    int base_health = 0;
    int base_max_health = 0;
    int score = 0;
    std::string level_name;
    std::vector<DeployCardPresentationInput> deploy_cards;
};

struct HudDeployCardModel {
    DeployCardViewModel card;
    bool enabled = false;
};

/// Coarse integrity band. The HUD maps each band onto a palette role, so the tier decision stays out of the node.
enum class IntegrityTier { INTACT, DAMAGED, CRITICAL };

struct HudWaveModel {
    std::string current_text;
    std::string total_text;
};

/// One segment per point of starting integrity, with the leading segment draining continuously so that damage
/// smaller than a whole segment still reads.
struct HudIntegrityModel {
    int segments = 0;
    double filled_segments = 0.0;
    IntegrityTier tier = IntegrityTier::INTACT;

    bool operator==(const HudIntegrityModel &) const = default;
};

struct HudModel {
    std::string energy_text;
    HudWaveModel wave;
    HudIntegrityModel integrity;
    std::string score_text;
    std::string level_text;
    bool level_visible = false;
    std::vector<HudDeployCardModel> deploy_cards;
};

class HudPresenter {
  public:
    HudPresenter() = delete;

    [[nodiscard]] static HudModel build(const HudPresentationInput &input);
};

} // namespace defn

#endif
