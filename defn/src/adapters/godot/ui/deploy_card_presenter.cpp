// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "deploy_card_presenter.h"

#include "deploy_card_view_model.h"
#include "godot_string.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"

#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>

namespace defn {

namespace {

Ref<Texture2D> load_portrait(const std::string &path) {
    const String texture_path = to_godot_string(path);
    if (texture_path.is_empty()) {
        return {};
    }
    auto *loader = ResourceLoader::get_singleton();
    if (loader == nullptr) {
        return {};
    }
    return loader->load(texture_path);
}

} // namespace

Button *DeployCardPresenter::create(const DeployCardViewModel &view_model, const Callable &pressed_action) {
    const CardNodes card = make_card({.variant = "deploy_card", .layout = CardLayout::Horizontal}, pressed_action);

    // The portrait leads, then the name over its cost: the same icon-then-text reading order the upgrade and
    // roster cards use, turned on its side because a deploy card is wide rather than tall.
    add_card_icon(card, make_card_portrait(load_portrait(view_model.portrait_path), UiThemeProvider::metric("deploy_card_portrait_size", 80)));

    card.text->add_child(make_card_title(to_godot_string(view_model.title)));

    // The cost carries the same bolt the HUD's energy plate does, tinted from the same `energy` role, rather
    // than an emoji drawn from whichever colour font the machine happens to ship.
    auto *cost_row = memnew(HBoxContainer);
    cost_row->set_name("Cost");
    cost_row->add_theme_constant_override("separation", UiThemeProvider::spacing("xs"));
    cost_row->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    cost_row->add_child(make_icon("energy", UiThemeProvider::metric("card_icon_size", 20)));
    cost_row->add_child(make_label(String::num_int64(view_model.cost), "card_cost"));
    card.text->add_child(cost_row);

    return card.button;
}

Button *DeployCardPresenter::create(const UnitConfig &config, const Callable &pressed_action) {
    return create(build_deploy_card_view_model(build_deploy_card_presentation_input(config)), pressed_action);
}

} // namespace defn
