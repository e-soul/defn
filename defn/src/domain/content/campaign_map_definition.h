// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef CAMPAIGN_MAP_DEFINITION_H
#define CAMPAIGN_MAP_DEFINITION_H

#include "content_values.h"

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

struct CampaignMapDefinition {
    /// Shown as the map's heading. Content owns what the campaign is called; the theme owns how it reads.
    std::string title;
    CampaignTextureDefinition background;
    std::vector<CampaignMapMissionDefinition> missions;
};

} // namespace defn

#endif
