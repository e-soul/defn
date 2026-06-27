#include "scene_navigator.h"

#include "progression_manager.h"

#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

constexpr auto MENU_SCENE_PATH = "res://scenes/menu.tscn";
constexpr auto GAME_SCENE_PATH = "res://scenes/game.tscn";

void change_scene(SceneTree *tree, const String &scene_path) {
    if (tree == nullptr) {
        return;
    }

    const Error err = tree->change_scene_to_file(scene_path);
    if (err != OK) {
        UtilityFunctions::printerr("SceneNavigator: Failed to change scene: ", scene_path, " error=", err);
    }
}

} // namespace

void SceneNavigator::go_to_main_menu(SceneTree *tree) { change_scene(tree, MENU_SCENE_PATH); }

void SceneNavigator::go_to_level(SceneTree *tree, const String &level_id) {
    if (auto *progression = CampaignService::get_singleton()) {
        progression->set_current_level_id(level_id);
    }
    change_scene(tree, GAME_SCENE_PATH);
}

void SceneNavigator::go_to_current_level(SceneTree *tree) { change_scene(tree, GAME_SCENE_PATH); }

void SceneNavigator::quit(SceneTree *tree) {
    if (tree != nullptr) {
        tree->quit();
    }
}

} // namespace defn