// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef CAMPAIGN_MAP_VIEW_MODEL_H
#define CAMPAIGN_MAP_VIEW_MODEL_H

#include "campaign_map_definition.h"
#include "level_definition.h"

#include <string>
#include <vector>

namespace defn {

enum class CampaignNodeState { LOCKED, AVAILABLE, FRONTIER, COMPLETED };
enum class CampaignRouteState { LOCKED, FRONTIER, COMPLETED };

struct CampaignLevelPresentationSource {
    std::string level_id;
    LevelDefinition definition;
    std::string requires_completed;
    bool unlocked = false;
    bool completed = false;
    bool frontier = false;
    int best_score = 0;
    int effective_starting_energy = 0;
    int effective_base_integrity = 0;
};

struct CampaignMissionViewModel {
    std::string level_id;
    int sequence_number = 0;
    std::string name;
    std::string tagline;
    CampaignPreviewDefinition preview;
    float position_x = 0.0F;
    float position_y = 0.0F;
    CampaignNodeState state = CampaignNodeState::LOCKED;
    std::string unlock_requirement;
    std::string threat_label;
    std::string duration_label;
    int wave_count = 0;
    int base_starting_energy = 0;
    int effective_starting_energy = 0;
    int base_integrity = 0;
    int effective_base_integrity = 0;
    int best_score = 0;
    CampaignMapAmbience ambience = CampaignMapAmbience::DUST;
    std::vector<std::string> enemy_labels;
};

struct CampaignRouteViewModel {
    std::size_t from_index = 0;
    std::size_t to_index = 0;
    CampaignRouteState state = CampaignRouteState::LOCKED;
};

struct CampaignMapViewModel {
    CampaignTextureDefinition background;
    std::vector<CampaignMissionViewModel> missions;
    std::vector<CampaignRouteViewModel> routes;
    std::string initial_selected_level_id;
    int completed_count = 0;
};

struct CampaignPreviewFrame {
    float draw_width = 0.0F;
    float draw_height = 0.0F;
    float origin_x = 0.0F;
    float origin_y = 0.0F;
};

class CampaignMapPresenter {
  public:
    CampaignMapPresenter() = delete;

    [[nodiscard]] static CampaignMapViewModel present(const CampaignMapDefinition &map, const std::vector<CampaignLevelPresentationSource> &levels);
    [[nodiscard]] static CampaignPreviewFrame frame_preview(float texture_width, float texture_height, float frame_width, float frame_height, float focus_x,
                                                            float focus_y, float zoom);
    [[nodiscard]] static std::string format_duration(double last_spawn_seconds);
    [[nodiscard]] static std::string title_case_id(const std::string &identifier);
};

} // namespace defn

#endif
