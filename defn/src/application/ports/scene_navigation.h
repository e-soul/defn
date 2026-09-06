// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SCENE_NAVIGATION_H
#define SCENE_NAVIGATION_H

#include <string>

namespace defn {

// `Endless` lands in the same scene `Level` does; what differs is the mode the campaign service is carrying, which
// the game scene reads when it composes the match.
enum class SceneNavigationDestination { MainMenu, CampaignMap, CurrentLevel, Level, Endless, Quit };

struct SceneNavigationRequest {
    SceneNavigationDestination destination = SceneNavigationDestination::MainMenu;
    std::string level_id;
};

} // namespace defn

#endif
