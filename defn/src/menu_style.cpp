#include "menu_style.h"

#include "variant_tools.h"

namespace defn {

namespace {

constexpr real_t DEFAULT_ALPHA = 1.0F;

} // namespace

Vector2 make_size(int width, int height) { return {static_cast<real_t>(width), static_cast<real_t>(height)}; }

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

Ref<StyleBoxFlat> make_style(const Dictionary &style_dict) {
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(parse_color_array(style_dict.get("bg_color", Array())));
    style->set_border_color(parse_color_array(style_dict.get("border_color", Array()), Color(0.4, 0.4, 0.5)));
    style->set_border_width_all(VariantTools::as_int(style_dict.get("border_width", 2)));
    style->set_corner_radius_all(VariantTools::as_int(style_dict.get("corner_radius", 8)));
    return style;
}

ButtonStyle build_button_style(const Dictionary &style_data) {
    ButtonStyle button_style;
    button_style.font_size = VariantTools::as_int(style_data.get("button_font_size", 32));
    button_style.minimum_size =
        make_size(VariantTools::as_int(style_data.get("button_min_width", 400)), VariantTools::as_int(style_data.get("button_min_height", 60)));
    button_style.separation = VariantTools::as_int(style_data.get("button_separation", 16));
    button_style.normal_style_data = style_data.get("normal", Dictionary());
    button_style.hover_style_data = style_data.get("hover", Dictionary());
    button_style.pressed_style_data = style_data.get("pressed", Dictionary());
    button_style.normal_font = parse_color_array(button_style.normal_style_data.get("font_color", Array()), Color(0.9, 0.9, 0.95));
    button_style.hover_font = parse_color_array(button_style.hover_style_data.get("font_color", Array()), Color(1, 1, 1));
    button_style.pressed_font = parse_color_array(button_style.pressed_style_data.get("font_color", Array()), Color(0.8, 0.8, 0.9));
    return button_style;
}

OptionsLayout build_options_layout(const Dictionary &options_style) {
    OptionsLayout options_layout;
    options_layout.label_font_size = VariantTools::as_int(options_style.get("label_font_size", 24));
    options_layout.label_minimum_size = make_size(VariantTools::as_int(options_style.get("label_min_width", 250)), 0);
    options_layout.control_minimum_size =
        make_size(VariantTools::as_int(options_style.get("control_min_width", 300)), VariantTools::as_int(options_style.get("control_min_height", 40)));
    options_layout.row_separation = VariantTools::as_int(options_style.get("row_separation", 12));
    options_layout.section_font_size = VariantTools::as_int(options_style.get("section_font_size", 28));
    options_layout.value_font_size = VariantTools::as_int(options_style.get("value_font_size", 20));
    options_layout.section_color = parse_color_array(options_style.get("section_font_color", Array()), Color(1, 0.85, 0.3));
    options_layout.label_color = parse_color_array(options_style.get("label_font_color", Array()), Color(0.85, 0.85, 0.9));
    options_layout.value_color = parse_color_array(options_style.get("value_font_color", Array()), Color(0.7, 0.8, 1.0));
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