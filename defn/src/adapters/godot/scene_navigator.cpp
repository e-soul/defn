// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "scene_navigator.h"

#include "godot_string.h"

#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

constexpr auto MENU_SCENE_PATH = "res://scenes/menu.tscn";
constexpr auto GAME_SCENE_PATH = "res://scenes/game.tscn";

bool change_scene(SceneTree *tree, const String &scene_path) {
    if (tree == nullptr) {
        return false;
    }

    const Error err = tree->change_scene_to_file(scene_path);
    if (err != OK) {
        UtilityFunctions::printerr("SceneNavigator: Failed to change scene: ", scene_path, " error=", err);
        return false;
    }
    return true;
}

} // namespace

bool SceneNavigator::campaign_map_requested_ = false;

void SceneNavigator::go_to_main_menu(SceneTree *tree) {
    campaign_map_requested_ = false;
    (void)change_scene(tree, MENU_SCENE_PATH);
}

void SceneNavigator::go_to_campaign_map(SceneTree *tree) {
    campaign_map_requested_ = true;
    if (!change_scene(tree, MENU_SCENE_PATH)) {
        campaign_map_requested_ = false;
    }
}

void SceneNavigator::go_to_level(SceneTree *tree, const String & /*level_id*/) {
    campaign_map_requested_ = false;
    (void)change_scene(tree, GAME_SCENE_PATH);
}

void SceneNavigator::go_to_current_level(SceneTree *tree) {
    campaign_map_requested_ = false;
    (void)change_scene(tree, GAME_SCENE_PATH);
}

void SceneNavigator::quit(SceneTree *tree) {
    campaign_map_requested_ = false;
    if (tree != nullptr) {
        tree->quit();
    }
}

void SceneNavigator::navigate(SceneTree *tree, const SceneNavigationRequest &request) {
    switch (request.destination) {
    case SceneNavigationDestination::MainMenu:
        go_to_main_menu(tree);
        break;
    case SceneNavigationDestination::CampaignMap:
        go_to_campaign_map(tree);
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

bool SceneNavigator::consume_campaign_map_request() {
    const bool requested = campaign_map_requested_;
    campaign_map_requested_ = false;
    return requested;
}

} // namespace defn
