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
    // Zero when this match has no cap, which is every authored campaign level; the readouts then hide themselves
    // rather than showing an infinity the player has to learn to ignore.
    int energy_cap = 0;
    // The ceiling only becomes a fact once the reserve has fallen to it, so the readout shows the cap from that
    // moment rather than from a starting grant that is legitimately above it.
    bool energy_ceiling_engaged = false;
    int supply_used = 0;
    int supply_cap = 0;
    std::string level_name;
    std::vector<DeployCardPresentationInput> deploy_cards;
};

struct HudDeployCardModel {
    DeployCardViewModel card;
    bool enabled = false;
};

/// Coarse integrity band. The HUD maps each band onto a palette role, so the tier decision stays out of the node.
enum class IntegrityTier { INTACT, DAMAGED, CRITICAL };

/// A count against a ceiling. Both the supply readout and the energy plate's cap suffix are this shape: a number,
/// a denominator that is only sometimes there, and whether the two have met.
struct HudCapModel {
    std::string cap_text;
    bool visible = false;
    bool at_cap = false;

    bool operator==(const HudCapModel &) const = default;
};

struct HudWaveModel {
    std::string current_text;
    std::string total_text;
    // False for an unbounded run, where the readout is a count rather than a fraction.
    bool total_visible = true;
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
    HudCapModel energy_cap;
    // How much of the line's supply is standing, as "3 / 12". Hidden entirely when the match is uncapped.
    std::string supply_text;
    HudCapModel supply;
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
