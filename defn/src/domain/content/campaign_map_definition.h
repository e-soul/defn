// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef CAMPAIGN_MAP_DEFINITION_H
#define CAMPAIGN_MAP_DEFINITION_H

#include "content_values.h"

#include <optional>
#include <string>
#include <vector>

namespace defn {

enum class CampaignMapAmbience { DUST, SPORES, MIST, SNOW, EMBERS, UNKNOWN };

struct CampaignTextureDefinition {
    std::string path;
};

struct CampaignPreviewDefinition {
    CampaignTextureDefinition texture;
    float focus_x = -1.0F;
    float focus_y = -1.0F;
    float node_zoom = 0.0F;
    float dossier_zoom = 0.0F;
};

struct CampaignMapMissionDefinition {
    std::string level_id;
    Vector2 position_normalized;
    std::string tagline;
    std::string threat_id;
    CampaignMapAmbience ambience = CampaignMapAmbience::UNKNOWN;
    CampaignPreviewDefinition preview;
};

// The endless beacon: a place on the map rather than a sixth mission. It is deliberately not a
// `CampaignMapMissionDefinition` -- the mission list drives the "N / 5 SECURED" count, the chained routes and the
// initial selection, and none of those should change because a mode was added.
struct CampaignEndlessDefinition {
    Vector2 position_normalized;
    std::string title;
    std::string tagline;
    std::string requires_completed;
    CampaignPreviewDefinition preview;
};

struct CampaignMapDefinition {
    /// Shown as the map's heading. Content owns what the campaign is called; the theme owns how it reads.
    std::string title;
    CampaignTextureDefinition background;
    std::vector<CampaignMapMissionDefinition> missions;
    std::optional<CampaignEndlessDefinition> endless;
};

} // namespace defn

#endif
