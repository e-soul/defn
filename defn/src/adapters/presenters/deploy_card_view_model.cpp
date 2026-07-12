#include "deploy_card_view_model.h"

#include "unit_definition.h"

#include <cctype>

namespace defn {

namespace {

std::string format_first_frame_path(const std::string &path_template) {
    if (path_template.empty()) {
        return {};
    }

    const size_t percent_index = path_template.find('%');
    if (percent_index == std::string::npos) {
        return path_template;
    }

    size_t cursor = percent_index + 1;
    const bool zero_padded = cursor < path_template.size() && path_template[cursor] == '0';
    if (zero_padded) {
        ++cursor;
    }

    int width = 0;
    while (cursor < path_template.size() && std::isdigit(static_cast<unsigned char>(path_template[cursor])) != 0) {
        width = (width * 10) + (path_template[cursor] - '0');
        ++cursor;
    }

    if (cursor >= path_template.size() || path_template[cursor] != 'd') {
        return path_template;
    }

    std::string frame_index = "0";
    if (zero_padded && width > static_cast<int>(frame_index.size())) {
        frame_index.insert(0, static_cast<size_t>(width - static_cast<int>(frame_index.size())), '0');
    }

    return path_template.substr(0, percent_index) + frame_index + path_template.substr(cursor + 1);
}

} // namespace

DeployCardPresentationInput build_deploy_card_presentation_input(const UnitConfig &config) {
    DeployCardPresentationInput input;
    input.unit_id = config.name;
    input.title = config.name;
    if (!input.title.empty()) {
        input.title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(input.title[0])));
    }
    input.cost = config.cost;
    input.animation_path_templates.reserve(config.animations.size());
    for (const auto &[name, animation] : config.animations) {
        input.animation_path_templates.emplace_back(name, animation.path_template);
    }
    return input;
}

DeployCardViewModel build_deploy_card_view_model(const DeployCardPresentationInput &input) {
    DeployCardViewModel view_model;
    view_model.unit_id = input.unit_id;
    view_model.title = input.title.empty() ? input.unit_id : input.title;
    view_model.cost = input.cost;
    for (const auto &[animation_name, path_template] : input.animation_path_templates) {
        if (animation_name == "shoot") {
            view_model.portrait_path = format_first_frame_path(path_template);
            break;
        }
    }
    return view_model;
}

} // namespace defn
