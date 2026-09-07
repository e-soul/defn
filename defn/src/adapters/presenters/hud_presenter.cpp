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

// An endless run authors no waves, so there is no denominator to show. "WAVE 12" rather than "WAVE 12/0" -- the
// unbounded form falls out of the wave count being zero, with no mode flag reaching the HUD.
HudWaveModel build_wave(int current_wave, int total_waves) {
    return {
        .current_text = std::to_string(std::max(0, current_wave)),
        .total_text = total_waves > 0 ? "/ " + std::to_string(total_waves) : std::string(),
        .total_visible = total_waves > 0,
    };
}

// A denominator that is only shown when it constrains anything. An uncapped match hides it, and an energy cap the
// player is still above is not yet a fact about their reserve -- see `MatchSession::apply_energy_ceiling`.
HudCapModel build_cap(int used, int cap, bool engaged) {
    if (cap <= 0 || !engaged) {
        return {};
    }
    return {.cap_text = "/ " + std::to_string(cap), .visible = true, .at_cap = used >= cap};
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
    model.energy_cap = build_cap(input.energy, input.energy_cap, input.energy_ceiling_engaged);
    // Supply has no equivalent of the energy ratchet: a cap on the line is in force from the first deployment, so
    // the readout appears as soon as the match declares one.
    model.supply_text = std::to_string(std::max(0, input.supply_used));
    model.supply = build_cap(input.supply_used, input.supply_cap, input.supply_cap > 0);
    model.wave = build_wave(input.current_wave, input.total_waves);
    model.integrity = build_integrity(input.base_health, input.base_max_health);
    model.score_text = std::to_string(std::max(0, input.score));
    model.level_text = to_upper_ascii(input.level_name);
    model.level_visible = !model.level_text.empty();

    model.deploy_cards.reserve(input.deploy_cards.size());
    for (const auto &card_input : input.deploy_cards) {
        DeployCardViewModel card = build_deploy_card_view_model(card_input);
        // A card the player cannot act on is disabled whichever rule stops them: at the supply cap every card is
        // unavailable, however much energy is banked.
        model.deploy_cards.push_back({
            .card = std::move(card),
            .enabled = input.energy >= card_input.cost && !model.supply.at_cap,
        });
    }

    return model;
}

} // namespace defn
