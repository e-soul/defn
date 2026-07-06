#ifndef SCENE_NAVIGATION_H
#define SCENE_NAVIGATION_H

#include <string>

namespace defn {

enum class SceneNavigationDestination { MainMenu, CurrentLevel, Level, Quit };

struct SceneNavigationRequest {
    SceneNavigationDestination destination = SceneNavigationDestination::MainMenu;
    std::string level_id;
};

} // namespace defn

#endif