#include "upgrade_card_presenter.h"

#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

namespace defn {

namespace {

Ref<StyleBoxFlat> make_card_style(const Color &background_color, const Color &border_color) {
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(background_color);
    style->set_border_width_all(2);
    style->set_border_color(border_color);
    style->set_corner_radius_all(12);
    style->set_content_margin_all(12);
    return style;
}

} // namespace

Button *UpgradeCardPresenter::create(const UpgradeCardViewModel &upgrade_card, bool selected, bool disabled, const Callable &pressed_action) {
    auto *button = memnew(Button);
    button->set_custom_minimum_size(Vector2(180, 220));
    button->set_focus_mode(Control::FOCUS_NONE);

    const Color background = selected ? Color(0.22, 0.2, 0.08, 0.98) : Color(0.1, 0.12, 0.18, 0.95);
    const Color border = selected ? Color(1.0, 0.82, 0.32) : Color(0.4, 0.46, 0.62);
    const Color hover_background = selected ? Color(0.24, 0.22, 0.1, 1.0) : Color(0.16, 0.19, 0.28, 0.98);
    const Color disabled_background = selected ? Color(0.22, 0.2, 0.08, 0.98) : Color(0.08, 0.09, 0.12, 0.85);
    const Color disabled_border = selected ? Color(1.0, 0.82, 0.32) : Color(0.28, 0.32, 0.4);

    button->add_theme_stylebox_override("normal", make_card_style(background, border));
    button->add_theme_stylebox_override("hover", make_card_style(hover_background, border));
    button->add_theme_stylebox_override("pressed", make_card_style(background.darkened(0.1), border));
    button->add_theme_stylebox_override("disabled", make_card_style(disabled_background, disabled_border));

    auto *content = memnew(VBoxContainer);
    content->set_anchors_preset(Control::PRESET_FULL_RECT);
    content->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    content->add_theme_constant_override("separation", 8);
    content->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    button->add_child(content);

    auto *emoji_label = memnew(Label);
    emoji_label->set_text(upgrade_card.emoji.is_empty() ? String("?") : upgrade_card.emoji);
    emoji_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    emoji_label->add_theme_font_size_override("font_size", 34);
    emoji_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    content->add_child(emoji_label);

    auto *name_label = memnew(Label);
    name_label->set_text(upgrade_card.name.is_empty() ? String("Upgrade") : upgrade_card.name);
    name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    name_label->add_theme_font_size_override("font_size", 18);
    name_label->add_theme_color_override("font_color", Color(0.96, 0.97, 1.0));
    name_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    content->add_child(name_label);

    auto *description_label = memnew(Label);
    description_label->set_text(upgrade_card.description);
    description_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    description_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    description_label->set_custom_minimum_size(Vector2(150, 0));
    description_label->add_theme_font_size_override("font_size", 14);
    description_label->add_theme_color_override("font_color", Color(0.82, 0.85, 0.9));
    description_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    content->add_child(description_label);

    if (pressed_action.is_valid()) {
        button->connect("pressed", pressed_action);
    }

    button->set_disabled(disabled);
    return button;
}

} // namespace defn