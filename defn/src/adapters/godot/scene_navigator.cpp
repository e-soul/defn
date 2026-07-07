#include "scene_navigator.h"

#include "progression_manager.h"

#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

constexpr auto MENU_SCENE_PATH = "res://scenes/menu.tscn";
constexpr auto GAME_SCENE_PATH = "res://scenes/game.tscn";

String to_godot_string(const std::string &value) { return {value.c_str()}; }

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
        progression->set_current_level_id_godot(level_id);
    }
    change_scene(tree, GAME_SCENE_PATH);
}

void SceneNavigator::go_to_current_level(SceneTree *tree) { change_scene(tree, GAME_SCENE_PATH); }

void SceneNavigator::quit(SceneTree *tree) {
    if (tree != nullptr) {
        tree->quit();
    }
}

void SceneNavigator::navigate(SceneTree *tree, const SceneNavigationRequest &request) {
    switch (request.destination) {
    case SceneNavigationDestination::MainMenu:
        go_to_main_menu(tree);
        break;
    case SceneNavigationDestination::CurrentLevel:
        go_to_current_level(tree);
        break;
    case SceneNavigationDestination::Level:
        go_to_level(tree, to_godot_string(request.level_id));
        break;
    case SceneNavigationDestination::Quit:
        quit(tree);
        break;
    }
}

} // namespace defn