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
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

namespace defn {

namespace {

real_t metric(const char *name, int fallback) { return static_cast<real_t>(UiThemeProvider::data().metric(name, fallback)); }

} // namespace

Button *DeployCardPresenter::create(const DeployCardViewModel &view_model, const Callable &pressed_action) {
    auto *button = make_button({}, "deploy_card", pressed_action);

    auto *content = memnew(HBoxContainer);
    content->set_anchors_preset(Control::PRESET_FULL_RECT);
    const real_t horizontal_inset = metric("deploy_card_horizontal_inset", 8);
    const real_t vertical_inset = metric("deploy_card_vertical_inset", 6);
    content->set_offset(Side::SIDE_LEFT, horizontal_inset);
    content->set_offset(Side::SIDE_RIGHT, -horizontal_inset);
    content->set_offset(Side::SIDE_TOP, vertical_inset);
    content->set_offset(Side::SIDE_BOTTOM, -vertical_inset);
    content->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    content->add_theme_constant_override("separation", UiThemeProvider::spacing("sm"));
    content->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    button->add_child(content);

    auto *portrait = memnew(TextureRect);
    const real_t portrait_size = metric("deploy_card_portrait_size", 80);
    portrait->set_custom_minimum_size(godot::Vector2(portrait_size, portrait_size));
    portrait->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
    portrait->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
    portrait->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
    portrait->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);

    const String shoot_frame_path = to_godot_string(view_model.portrait_path);
    if (!shoot_frame_path.is_empty()) {
        if (auto *loader = ResourceLoader::get_singleton(); loader != nullptr) {
            Ref<Texture2D> texture = loader->load(shoot_frame_path);
            if (texture.is_valid()) {
                portrait->set_texture(texture);
            }
        }
    }
    content->add_child(portrait);

    auto *text_column = memnew(VBoxContainer);
    text_column->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    text_column->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
    text_column->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    text_column->add_theme_constant_override("separation", 0);
    text_column->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    content->add_child(text_column);

    auto *name_label = make_label(to_godot_string(view_model.title), "deploy_card_title");
    name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_LEFT);
    name_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    text_column->add_child(name_label);

    auto *cost_label = make_label(vformat(String::utf8("\u26A1 %d"), view_model.cost), "card_cost");
    cost_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_LEFT);
    cost_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    text_column->add_child(cost_label);

    return button;
}

Button *DeployCardPresenter::create(const UnitConfig &config, const Callable &pressed_action) {
    return create(build_deploy_card_view_model(build_deploy_card_presentation_input(config)), pressed_action);
}

} // namespace defn
