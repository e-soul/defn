// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "deploy_card_presenter.h"

#include "deploy_card_view_model.h"
#include "godot_string.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"

#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>

#include <algorithm>
#include <cmath>

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

    auto *title = make_card_title(to_godot_string(view_model.title));
    title->set_name("CardTitle");
    card.text->add_child(title);

    // The cost carries the same bolt the HUD's energy plate does, tinted from the same `energy` role, rather
    // than an emoji drawn from whichever colour font the machine happens to ship.
    auto *cost_row = memnew(HBoxContainer);
    cost_row->set_name("Cost");
    cost_row->add_theme_constant_override("separation", UiThemeProvider::spacing("xs"));
    cost_row->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    cost_row->add_child(make_icon("energy", UiThemeProvider::metric("card_icon_size", 20)));
    auto *cost = make_label(String::num_int64(view_model.cost), "card_cost");
    cost->set_name("CardCost");
    cost_row->add_child(cost);
    card.text->add_child(cost_row);

    return card.button;
}

Button *DeployCardPresenter::create(const UnitConfig &config, const Callable &pressed_action) {
    return create(build_deploy_card_view_model(build_deploy_card_presentation_input(config)), pressed_action);
}

HudSizing DeployCardPresenter::resolve_sizing(const UiViewportMetrics &viewport) {
    return resolve_hud_sizing(viewport, {.width = static_cast<float>(UiThemeProvider::metric("mobile_deploy_card_max_width", 180)),
                                         .height = static_cast<float>(UiThemeProvider::metric("mobile_deploy_card_height", 44)),
                                         .portrait_size = static_cast<float>(UiThemeProvider::metric("mobile_deploy_card_portrait_size", 28)),
                                         .inset = static_cast<float>(UiThemeProvider::metric("mobile_deploy_card_inset", 5))});
}

void DeployCardPresenter::apply_sizing(Button *button, const HudSizing &sizing) {
    const auto *variant = UiThemeProvider::data().find_button("deploy_card");
    const auto default_width = static_cast<float>(variant == nullptr ? 190 : variant->min_width);
    const auto default_height = static_cast<float>(variant == nullptr ? 110 : variant->min_height);
    const float width = sizing.responsive ? sizing.card_width : default_width;
    const float height = sizing.responsive ? sizing.card_height : default_height;
    button->set_custom_minimum_size({width, height});
    auto *portrait = Object::cast_to<TextureRect>(button->find_child("CardPortrait", true, false));
    const float portrait_size = sizing.responsive ? sizing.portrait_size : UiThemeProvider::metric("deploy_card_portrait_size", 80);
    portrait->set_custom_minimum_size({portrait_size, portrait_size});
    const int inset = sizing.responsive ? static_cast<int>(std::round(sizing.card_inset)) : UiThemeProvider::spacing("sm");
    auto *margins = Object::cast_to<MarginContainer>(button->find_child("CardMargins", true, false));
    for (const char *side : {"margin_left", "margin_top", "margin_right", "margin_bottom"}) {
        margins->add_theme_constant_override(side, inset);
    }
    auto *body = Object::cast_to<BoxContainer>(button->find_child("CardBody", true, false));
    body->add_theme_constant_override("separation", inset);
    auto *title = Object::cast_to<Label>(button->find_child("CardTitle", true, false));
    auto *cost_row = Object::cast_to<HBoxContainer>(button->find_child("Cost", true, false));
    auto *text = Object::cast_to<BoxContainer>(button->find_child("CardText", true, false));
    // Keep the portrait, expanding name, and fixed-width cost on one line on phones. Moving the existing
    // cost row preserves the button, its signals, focus, and affordability state across viewport changes.
    BoxContainer *cost_parent = sizing.responsive ? body : text;
    if (cost_row->get_parent() != cost_parent) {
        cost_row->reparent(cost_parent, false);
    }
    cost_row->set_v_size_flags(sizing.responsive ? Control::SIZE_SHRINK_CENTER : Control::SIZE_FILL);
    cost_row->add_theme_constant_override("separation", sizing.responsive ? inset : UiThemeProvider::spacing("xs"));
    auto *cost_icon = Object::cast_to<TextureRect>(cost_row->get_child(0));
    const float icon_size = sizing.responsive ? 12.0F / sizing.pixels_per_unit : UiThemeProvider::metric("card_icon_size", 20);
    cost_icon->set_custom_minimum_size({icon_size, icon_size});
    // The card button owns its child layout; its text needs a real width budget so a long title ellipsizes
    // without displacing the cost or portrait. A Button does not propagate its children's minimum size.
    title->set_custom_minimum_size({0.0F, 0.0F});
    if (sizing.responsive) {
        const Ref<Font> font = title->get_theme_font("font");
        const int font_size = UiThemeProvider::font_size("body");
        const float name_width = std::ceil(font->get_string_size(title->get_text(), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x);
        auto *cost_label = Object::cast_to<Label>(cost_row->get_child(1));
        const float cost_width =
            std::ceil(cost_label->get_theme_font("font")->get_string_size(cost_label->get_text(), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x);
        const float fixed_width = portrait_size + icon_size + cost_width + static_cast<float>(5 * inset);
        const float title_width = std::min(name_width, std::max(0.0F, width - fixed_width));
        title->set_custom_minimum_size({title_width, 0.0F});
        button->set_custom_minimum_size({fixed_width + title_width, height});
    }
}

} // namespace defn
