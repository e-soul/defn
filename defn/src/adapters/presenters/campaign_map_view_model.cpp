// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "campaign_map_view_model.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_set>

namespace defn {

namespace {

const CampaignLevelPresentationSource *find_level(const std::vector<CampaignLevelPresentationSource> &levels, const std::string &level_id) {
    const auto found = std::ranges::find_if(levels, [&level_id](const CampaignLevelPresentationSource &level) { return level.level_id == level_id; });
    return found == levels.end() ? nullptr : &*found;
}

CampaignNodeState node_state(const CampaignLevelPresentationSource &level) {
    if (level.completed) {
        return CampaignNodeState::COMPLETED;
    }
    if (level.frontier && level.unlocked) {
        return CampaignNodeState::FRONTIER;
    }
    return level.unlocked ? CampaignNodeState::AVAILABLE : CampaignNodeState::LOCKED;
}

std::string threat_label(const std::string &threat_id) { return CampaignMapPresenter::title_case_id(threat_id); }

double last_spawn_time(const LevelDefinition &definition) {
    double result = 0.0;
    for (const auto &wave : definition.waves) {
        for (const auto &spawn : wave.spawns) {
            result = std::max(result, spawn.time);
        }
    }
    return result;
}

std::vector<std::string> enemy_labels(const LevelDefinition &definition) {
    std::vector<std::string> result;
    std::unordered_set<std::string> seen;
    for (const auto &wave : definition.waves) {
        for (const auto &spawn : wave.spawns) {
            if (seen.insert(spawn.type).second) {
                result.push_back(CampaignMapPresenter::title_case_id(spawn.type));
            }
        }
    }
    return result;
}

CampaignMissionViewModel build_mission_view_model(const CampaignMapMissionDefinition &mission, const CampaignLevelPresentationSource &source,
                                                  std::size_t index) {
    return {
        .level_id = mission.level_id,
        .sequence_number = static_cast<int>(index + 1),
        .name = source.definition.name.empty() ? CampaignMapPresenter::title_case_id(mission.level_id) : source.definition.name,
        .tagline = mission.tagline.empty() ? "Operational intelligence unavailable." : mission.tagline,
        .preview = mission.preview,
        .position_x = mission.position_normalized.x,
        .position_y = mission.position_normalized.y,
        .state = node_state(source),
        .unlock_requirement = source.requires_completed.empty()
                                  ? "Campaign route unavailable."
                                  : "Secure " + CampaignMapPresenter::title_case_id(source.requires_completed) + " to open this operation.",
        .threat_label = threat_label(mission.threat_id),
        .duration_label = CampaignMapPresenter::format_duration(last_spawn_time(source.definition)),
        .wave_count = static_cast<int>(source.definition.waves.size()),
        .base_starting_energy = source.definition.starting_core_resource,
        .effective_starting_energy = source.effective_starting_energy,
        .base_integrity = source.definition.base_integrity,
        .effective_base_integrity = source.effective_base_integrity,
        .best_score = source.best_score,
        .ambience = mission.ambience,
        .enemy_labels = enemy_labels(source.definition),
    };
}

std::string choose_initial_selection(const std::vector<CampaignMissionViewModel> &missions) {
    const auto frontier = std::ranges::find_if(missions, [](const CampaignMissionViewModel &mission) { return mission.state == CampaignNodeState::FRONTIER; });
    if (frontier != missions.end()) {
        return frontier->level_id;
    }
    const auto available = std::ranges::find_if(missions, [](const CampaignMissionViewModel &mission) {
        return mission.state == CampaignNodeState::AVAILABLE || mission.state == CampaignNodeState::COMPLETED;
    });
    if (available != missions.end()) {
        return available->level_id;
    }
    return missions.empty() ? std::string() : missions.front().level_id;
}

CampaignRouteState route_state(const CampaignMissionViewModel &source, const CampaignMissionViewModel &destination) {
    if (destination.state == CampaignNodeState::COMPLETED) {
        return CampaignRouteState::COMPLETED;
    }
    if (source.state == CampaignNodeState::COMPLETED &&
        (destination.state == CampaignNodeState::AVAILABLE || destination.state == CampaignNodeState::FRONTIER)) {
        return CampaignRouteState::FRONTIER;
    }
    return CampaignRouteState::LOCKED;
}

std::vector<CampaignRouteViewModel> build_routes(const std::vector<CampaignMissionViewModel> &missions) {
    std::vector<CampaignRouteViewModel> result;
    for (std::size_t index = 1; index < missions.size(); ++index) {
        result.push_back({.from_index = index - 1, .to_index = index, .state = route_state(missions[index - 1], missions[index])});
    }
    return result;
}

} // namespace

CampaignMapViewModel CampaignMapPresenter::present(const CampaignMapDefinition &map, const std::vector<CampaignLevelPresentationSource> &levels) {
    CampaignMapViewModel result;
    result.title = map.title;
    result.background = map.background;
    result.missions.reserve(map.missions.size());

    for (std::size_t index = 0; index < map.missions.size(); ++index) {
        const CampaignMapMissionDefinition &mission = map.missions[index];
        const CampaignLevelPresentationSource *source = find_level(levels, mission.level_id);
        if (source == nullptr) {
            continue;
        }

        CampaignMissionViewModel mission_view_model = build_mission_view_model(mission, *source, index);
        if (mission_view_model.state == CampaignNodeState::COMPLETED) {
            ++result.completed_count;
        }
        result.missions.push_back(std::move(mission_view_model));
    }

    result.initial_selected_level_id = choose_initial_selection(result.missions);
    result.routes = build_routes(result.missions);
    return result;
}

CampaignPreviewFrame CampaignMapPresenter::frame_preview(float texture_width, float texture_height, float frame_width, float frame_height, float focus_x,
                                                         float focus_y, float zoom) {
    if (texture_width <= 0.0F || texture_height <= 0.0F || frame_width <= 0.0F || frame_height <= 0.0F) {
        return {};
    }
    const float safe_zoom = std::max(zoom, 1.0F);
    const float cover = std::max(frame_width / texture_width, frame_height / texture_height) * safe_zoom;
    const float draw_width = texture_width * cover;
    const float draw_height = texture_height * cover;
    const float desired_x = (frame_width * 0.5F) - (std::clamp(focus_x, 0.0F, 1.0F) * draw_width);
    const float desired_y = (frame_height * 0.5F) - (std::clamp(focus_y, 0.0F, 1.0F) * draw_height);
    return {
        .draw_width = draw_width,
        .draw_height = draw_height,
        .origin_x = std::clamp(desired_x, frame_width - draw_width, 0.0F),
        .origin_y = std::clamp(desired_y, frame_height - draw_height, 0.0F),
    };
}

std::string CampaignMapPresenter::format_duration(double last_spawn_seconds) {
    const double buffered = std::max(last_spawn_seconds, 0.0) + 15.0;
    if (buffered < 70.0) {
        return "~1 MIN";
    }
    if (buffered <= 120.0) {
        return "~2 MIN";
    }
    if (buffered <= 180.0) {
        return "~3 MIN";
    }
    return "3+ MIN";
}

std::string CampaignMapPresenter::title_case_id(const std::string &identifier) {
    std::string result = identifier;
    bool capitalize = true;
    for (char &character : result) {
        if (character == '_') {
            character = ' ';
            capitalize = true;
            continue;
        }
        const auto value = static_cast<unsigned char>(character);
        character = static_cast<char>(capitalize ? std::toupper(value) : std::tolower(value));
        capitalize = false;
    }
    return result;
}

} // namespace defn
