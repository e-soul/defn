#include "deploy_card_presenter.h"

#include "deploy_card_view_model.h"
#include "godot_string.h"

#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

#include <cctype>

namespace defn {

namespace {

DeployCardPresentationInput to_presentation_input(const UnitConfig &config) {
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

Ref<StyleBoxFlat> make_card_style(const Color &background_color, const Color &border_color) {
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(background_color);
    style->set_border_width_all(2);
    style->set_border_color(border_color);
    style->set_corner_radius_all(8);
    return style;
}

} // namespace

Button *DeployCardPresenter::create(const DeployCardViewModel &view_model, const Callable &pressed_action) {
    auto *button = memnew(Button);
    button->set_custom_minimum_size(Vector2(190, 110));
    button->set_focus_mode(Control::FOCUS_NONE);
    button->add_theme_stylebox_override("normal", make_card_style(Color(0.12, 0.12, 0.18, 0.9), Color(0.4, 0.4, 0.5)));
    button->add_theme_stylebox_override("hover", make_card_style(Color(0.18, 0.18, 0.28, 0.95), Color(0.6, 0.6, 0.8)));
    button->add_theme_stylebox_override("pressed", make_card_style(Color(0.08, 0.08, 0.14, 0.95), Color(0.5, 0.5, 0.7)));
    button->add_theme_stylebox_override("disabled", make_card_style(Color(0.08, 0.08, 0.1, 0.7), Color(0.25, 0.25, 0.3)));

    auto *content = memnew(HBoxContainer);
    content->set_anchors_preset(Control::PRESET_FULL_RECT);
    content->set_offset(Side::SIDE_LEFT, 8.0);
    content->set_offset(Side::SIDE_RIGHT, -8.0);
    content->set_offset(Side::SIDE_TOP, 6.0);
    content->set_offset(Side::SIDE_BOTTOM, -6.0);
    content->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    content->add_theme_constant_override("separation", 8);
    content->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    button->add_child(content);

    auto *portrait = memnew(TextureRect);
    portrait->set_custom_minimum_size(Vector2(80, 80));
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

    auto *name_label = memnew(Label);
    name_label->set_text(to_godot_string(view_model.title));
    name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_LEFT);
    name_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    name_label->add_theme_font_size_override("font_size", 14);
    name_label->add_theme_color_override("font_color", Color(0.9, 0.9, 0.95));
    name_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    text_column->add_child(name_label);

    auto *cost_label = memnew(Label);
    cost_label->set_text(vformat(String::utf8("\u26A1 %d"), view_model.cost));
    cost_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_LEFT);
    cost_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    cost_label->add_theme_font_size_override("font_size", 13);
    cost_label->add_theme_color_override("font_color", Color(0.3, 0.7, 1.0));
    cost_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    text_column->add_child(cost_label);

    if (pressed_action.is_valid()) {
        button->connect("pressed", pressed_action);
    }

    return button;
}

Button *DeployCardPresenter::create(const UnitConfig &config, const Callable &pressed_action) {
    return create(build_deploy_card_view_model(to_presentation_input(config)), pressed_action);
}

} // namespace defn