#ifndef MENU_FLOW_USE_CASE_H
#define MENU_FLOW_USE_CASE_H

#include "menu_intents.h"
#include "progression_service.h"
#include "scene_navigation.h"

#include <optional>
#include <string>

namespace defn {

enum class MenuFlowView { None, Menu, LevelSelect, Progression };

struct MenuFlowResult {
    MenuFlowView view = MenuFlowView::None;
    std::string menu_name;
    std::optional<SceneNavigationRequest> navigation;
    bool resume = false;
};

class MenuFlowUseCase {
  public:
    explicit MenuFlowUseCase(ProgressionService *progression = nullptr);

    [[nodiscard]] static MenuFlowResult handle(const MenuIntent &intent);
    [[nodiscard]] MenuFlowResult select_level(const std::string &level_id) const;
    [[nodiscard]] static MenuFlowResult request_main_menu();

  private:
    ProgressionService *progression_ = nullptr;
};

} // namespace defn

#endif