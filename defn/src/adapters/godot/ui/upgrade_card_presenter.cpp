#include "upgrade_card_presenter.h"

#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

constexpr real_t UPGRADE_CARD_WIDTH = 180.0;
constexpr real_t UPGRADE_CARD_HEIGHT = 220.0;
constexpr real_t UPGRADE_CARD_CONTENT_MARGIN = 18.0;
constexpr real_t UPGRADE_CARD_TEXT_WIDTH = UPGRADE_CARD_WIDTH - (UPGRADE_CARD_CONTENT_MARGIN * 2.0);
constexpr real_t UPGRADE_CARD_TITLE_MIN_HEIGHT = 44.0;

String to_godot_string(const std::string &value) { return {value.c_str()}; }

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
    button->set_custom_minimum_size(Vector2(UPGRADE_CARD_WIDTH, UPGRADE_CARD_HEIGHT));
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

    auto *content_margin = memnew(MarginContainer);
    content_margin->set_anchors_preset(Control::PRESET_FULL_RECT);
    content_margin->add_theme_constant_override("margin_left", static_cast<int>(UPGRADE_CARD_CONTENT_MARGIN));
    content_margin->add_theme_constant_override("margin_top", static_cast<int>(UPGRADE_CARD_CONTENT_MARGIN));
    content_margin->add_theme_constant_override("margin_right", static_cast<int>(UPGRADE_CARD_CONTENT_MARGIN));
    content_margin->add_theme_constant_override("margin_bottom", static_cast<int>(UPGRADE_CARD_CONTENT_MARGIN));
    content_margin->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    button->add_child(content_margin);

    auto *content = memnew(VBoxContainer);
    content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    content->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    content->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    content->add_theme_constant_override("separation", 8);
    content->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    content_margin->add_child(content);

    auto *emoji_label = memnew(Label);
    emoji_label->set_text(upgrade_card.emoji.empty() ? String("?") : to_godot_string(upgrade_card.emoji));
    emoji_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    emoji_label->add_theme_font_size_override("font_size", 34);
    emoji_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    content->add_child(emoji_label);

    if (upgrade_card.owned_count > 1) {
        auto *count_label = memnew(Label);
        count_label->set_text(vformat("x%d", upgrade_card.owned_count));
        count_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        count_label->add_theme_font_size_override("font_size", 18);
        count_label->add_theme_color_override("font_color", Color(1.0, 0.85, 0.3));
        count_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        content->add_child(count_label);
    }

    auto *name_label = memnew(Label);
    name_label->set_text(upgrade_card.name.empty() ? String("Upgrade") : to_godot_string(upgrade_card.name));
    name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    name_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
    name_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    name_label->set_custom_minimum_size(Vector2(UPGRADE_CARD_TEXT_WIDTH, UPGRADE_CARD_TITLE_MIN_HEIGHT));
    name_label->add_theme_font_size_override("font_size", 18);
    name_label->add_theme_color_override("font_color", Color(0.96, 0.97, 1.0));
    name_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    content->add_child(name_label);

    auto *description_label = memnew(Label);
    description_label->set_text(to_godot_string(upgrade_card.description));
    description_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    description_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    description_label->set_custom_minimum_size(Vector2(UPGRADE_CARD_TEXT_WIDTH, 0));
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