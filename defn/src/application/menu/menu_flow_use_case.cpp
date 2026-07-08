#include "menu_flow_use_case.h"

namespace defn {

namespace {

MenuFlowResult navigation_result(SceneNavigationDestination destination, std::string level_id = {}) {
    return {
        .navigation = SceneNavigationRequest{.destination = destination, .level_id = std::move(level_id)},
    };
}

} // namespace

MenuFlowUseCase::MenuFlowUseCase(ProgressionService *progression) : progression_(progression) {}

MenuFlowResult MenuFlowUseCase::handle(const MenuIntent &intent) {
    switch (intent.type) {
    case MenuIntentType::GotoMenu:
        return {.view = MenuFlowView::Menu, .menu_name = intent.target};
    case MenuIntentType::ShowLevelSelect:
        return {.view = MenuFlowView::LevelSelect};
    case MenuIntentType::ShowProgression:
        return {.view = MenuFlowView::Progression};
    case MenuIntentType::StartGame:
        return navigation_result(SceneNavigationDestination::CurrentLevel);
    case MenuIntentType::Quit:
        return navigation_result(SceneNavigationDestination::Quit);
    case MenuIntentType::Resume:
        return {.resume = true};
    case MenuIntentType::MainMenu:
        return request_main_menu();
    case MenuIntentType::None:
        break;
    }

    return {};
}

MenuFlowResult MenuFlowUseCase::select_level(const std::string &level_id) const {
    if (level_id.empty() || progression_ == nullptr || !progression_->select_level(level_id)) {
        return {};
    }

    return navigation_result(SceneNavigationDestination::Level, level_id);
}

MenuFlowResult MenuFlowUseCase::request_main_menu() { return navigation_result(SceneNavigationDestination::MainMenu); }

} // namespace defn