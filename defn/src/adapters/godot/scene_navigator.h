#ifndef SCENE_NAVIGATOR_H
#define SCENE_NAVIGATOR_H

#include "scene_navigation.h"

#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/string.hpp>

namespace defn {

using namespace godot;

class SceneNavigator {
  public:
    SceneNavigator() = delete;

    static void go_to_main_menu(SceneTree *tree);
    static void go_to_level(SceneTree *tree, const String &level_id);
    static void go_to_current_level(SceneTree *tree);
    static void quit(SceneTree *tree);
    static void navigate(SceneTree *tree, const SceneNavigationRequest &request);
};

} // namespace defn

#endif