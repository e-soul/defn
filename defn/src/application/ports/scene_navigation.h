// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SCENE_NAVIGATION_H
#define SCENE_NAVIGATION_H

#include <string>

namespace defn {

enum class SceneNavigationDestination { MainMenu, CampaignMap, CurrentLevel, Level, Quit };

struct SceneNavigationRequest {
    SceneNavigationDestination destination = SceneNavigationDestination::MainMenu;
    std::string level_id;
};

} // namespace defn

#endif
