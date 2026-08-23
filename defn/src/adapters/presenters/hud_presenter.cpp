// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "hud_presenter.h"

#include "match_session.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace defn {

namespace {

std::string to_upper_ascii(const std::string &text) {
    std::string upper = text;
    for (char &character : upper) {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
    return upper;
}

HudWaveModel build_wave(int current_wave, int total_waves) {
    return {
        .current_text = std::to_string(std::max(0, current_wave)),
        .total_text = "/ " + std::to_string(std::max(0, total_waves)),
    };
}

IntegrityTier integrity_tier(double filled_segments, int segments) {
    if (filled_segments >= static_cast<double>(segments)) {
        return IntegrityTier::INTACT;
    }
    if (segments <= 1 || filled_segments <= 1.0) {
        return IntegrityTier::CRITICAL;
    }
    return IntegrityTier::DAMAGED;
}

/// Health worth one integrity segment. The bar is a readout of the domain's own heart granularity, so it takes
/// the figure from there rather than keeping a second copy that could drift.
constexpr int HEALTH_PER_SEGMENT = MatchSession::BASE_HEALTH_PER_HEART;

HudIntegrityModel build_integrity(int health, int max_health) {
    HudIntegrityModel model;
    const int capacity = std::max(0, max_health);
    if (capacity == 0) {
        return model;
    }

    model.segments = (capacity + HEALTH_PER_SEGMENT - 1) / HEALTH_PER_SEGMENT;
    model.filled_segments = static_cast<double>(std::clamp(health, 0, capacity)) / HEALTH_PER_SEGMENT;
    model.tier = integrity_tier(model.filled_segments, model.segments);
    return model;
}

} // namespace

HudModel HudPresenter::build(const HudPresentationInput &input) {
    HudModel model;
    model.energy_text = std::to_string(std::max(0, input.energy));
    model.wave = build_wave(input.current_wave, input.total_waves);
    model.integrity = build_integrity(input.base_health, input.base_max_health);
    model.score_text = std::to_string(std::max(0, input.score));
    model.level_text = to_upper_ascii(input.level_name);
    model.level_visible = !model.level_text.empty();

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
