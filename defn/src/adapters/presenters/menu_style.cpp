#include "menu_style.h"

#include "variant_tools.h"

namespace defn {

namespace {

constexpr real_t DEFAULT_ALPHA = 1.0F;

} // namespace

Vector2 make_size(int width, int height) { return {static_cast<real_t>(width), static_cast<real_t>(height)}; }

Color to_godot_color(const ContentColor &color) { return {color.r, color.g, color.b, color.a}; }

Color parse_color_array(const Array &arr, const Color &fallback) {
    if (arr.size() >= 3) {
        const auto red = VariantTools::as_real(arr[0]);
        const auto green = VariantTools::as_real(arr[1]);
        const auto blue = VariantTools::as_real(arr[2]);
        const auto alpha = arr.size() >= 4 ? VariantTools::as_real(arr[3]) : DEFAULT_ALPHA;
        return {red, green, blue, alpha};
    }

    return fallback;
}

Ref<StyleBoxFlat> make_style(const MenuStyleBoxData &style_data) {
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(to_godot_color(style_data.bg_color));
    style->set_border_color(to_godot_color(style_data.border_color));
    style->set_border_width_all(style_data.border_width);
    style->set_corner_radius_all(style_data.corner_radius);
    return style;
}

ButtonStyle build_button_style(const MenuStyleData &style_data) {
    ButtonStyle button_style;
    button_style.font_size = style_data.button_font_size;
    button_style.minimum_size = make_size(style_data.button_min_width, style_data.button_min_height);
    button_style.separation = style_data.button_separation;
    button_style.normal_style_data = style_data.normal;
    button_style.hover_style_data = style_data.hover;
    button_style.pressed_style_data = style_data.pressed;
    button_style.normal_font = to_godot_color(button_style.normal_style_data.font_color);
    button_style.hover_font = to_godot_color(button_style.hover_style_data.font_color);
    button_style.pressed_font = to_godot_color(button_style.pressed_style_data.font_color);
    return button_style;
}

OptionsLayout build_options_layout(const MenuOptionsStyleData &options_style) {
    OptionsLayout options_layout;
    options_layout.label_font_size = options_style.label_font_size;
    options_layout.label_minimum_size = make_size(options_style.label_min_width, 0);
    options_layout.control_minimum_size = make_size(options_style.control_min_width, options_style.control_min_height);
    options_layout.row_separation = options_style.row_separation;
    options_layout.section_font_size = options_style.section_font_size;
    options_layout.value_font_size = options_style.value_font_size;
    options_layout.section_color = to_godot_color(options_style.section_font_color);
    options_layout.label_color = to_godot_color(options_style.label_font_color);
    options_layout.value_color = to_godot_color(options_style.value_font_color);
    return options_layout;
}

void apply_button_theme(Control *control, const ButtonStyle &button_style, int font_size) {
    if (control == nullptr) {
        return;
    }

    const int resolved_font_size = font_size > 0 ? font_size : button_style.font_size;
    control->add_theme_font_size_override("font_size", resolved_font_size);
    control->add_theme_stylebox_override("normal", make_style(button_style.normal_style_data));
    control->add_theme_stylebox_override("hover", make_style(button_style.hover_style_data));
    control->add_theme_stylebox_override("pressed", make_style(button_style.pressed_style_data));
    control->add_theme_color_override("font_color", button_style.normal_font);
    control->add_theme_color_override("font_hover_color", button_style.hover_font);
    control->add_theme_color_override("font_pressed_color", button_style.pressed_font);
}

void apply_disabled_style(BaseButton *button, bool enabled) {
    if (button == nullptr) {
        return;
    }

    button->set_disabled(!enabled);
    button->set_modulate(enabled ? Color(1, 1, 1, 1) : Color(0.5, 0.5, 0.5, 0.7));
}

} // namespace defn