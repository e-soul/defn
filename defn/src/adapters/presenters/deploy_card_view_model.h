#ifndef DEPLOY_CARD_VIEW_MODEL_H
#define DEPLOY_CARD_VIEW_MODEL_H

#include <string>
#include <utility>
#include <vector>

namespace defn {

struct UnitConfig;

struct DeployCardPresentationInput {
    std::string unit_id;
    std::string title;
    int cost = 0;
    std::vector<std::pair<std::string, std::string>> animation_path_templates;
};

struct DeployCardViewModel {
    std::string unit_id;
    std::string title;
    int cost = 0;
    std::string portrait_path;
};

[[nodiscard]] DeployCardViewModel build_deploy_card_view_model(const DeployCardPresentationInput &input);
[[nodiscard]] DeployCardPresentationInput build_deploy_card_presentation_input(const UnitConfig &config);

} // namespace defn

#endif
