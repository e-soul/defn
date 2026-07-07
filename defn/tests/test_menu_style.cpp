#include "test_harness.h"

#include "menu_style.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/core/memory.hpp>

namespace defn {

namespace {

Array make_array(std::initializer_list<Variant> values) {
    Array array;
    for (const Variant &value : values) {
        array.push_back(value);
    }
    return array;
}

void check_color_close(const Color &actual, const Color &expected) {
    DEFN_CHECK_CLOSE(actual.r, expected.r, 0.001);
    DEFN_CHECK_CLOSE(actual.g, expected.g, 0.001);
    DEFN_CHECK_CLOSE(actual.b, expected.b, 0.001);
    DEFN_CHECK_CLOSE(actual.a, expected.a, 0.001);
}

ContentColor make_content_color(float red, float green, float blue, float alpha) { return {.r = red, .g = green, .b = blue, .a = alpha}; }

} // namespace

DEFN_TEST(menu_style_parse_color_array_handles_alpha_and_fallback) {
    check_color_close(parse_color_array(make_array({0.1, 0.2, 0.3})), Color(0.1F, 0.2F, 0.3F, 1.0F));
    check_color_close(parse_color_array(make_array({0.4, 0.5, 0.6, 0.7})), Color(0.4F, 0.5F, 0.6F, 0.7F));
    check_color_close(parse_color_array(make_array({0.8, 0.9}), Color(0.2F, 0.3F, 0.4F, 0.5F)), Color(0.2F, 0.3F, 0.4F, 0.5F));
}

DEFN_TEST(menu_style_make_style_maps_plain_style_values) {
    MenuStyleBoxData style_data;
    style_data.bg_color = make_content_color(0.2F, 0.3F, 0.4F, 0.5F);
    style_data.border_color = make_content_color(0.6F, 0.7F, 0.8F, 0.9F);
    style_data.border_width = 5;
    style_data.corner_radius = 12;

    const Ref<StyleBoxFlat> style = make_style(style_data);
    DEFN_REQUIRE(style.is_valid());
    check_color_close(style->get_bg_color(), Color(0.2F, 0.3F, 0.4F, 0.5F));
    check_color_close(style->get_border_color(), Color(0.6F, 0.7F, 0.8F, 0.9F));
    DEFN_CHECK_EQ(style->get_border_width(SIDE_LEFT), 5);
    DEFN_CHECK_EQ(style->get_corner_radius(CORNER_TOP_LEFT), 12);
}

DEFN_TEST(menu_style_builds_button_style_with_defaults_and_overrides) {
    MenuStyleData style_data;
    style_data.button_font_size = 18;
    style_data.button_min_width = 220;
    style_data.button_min_height = 44;
    style_data.button_separation = 9;
    style_data.normal.font_color = make_content_color(0.1F, 0.2F, 0.3F, 0.4F);
    style_data.hover.font_color = make_content_color(0.5F, 0.6F, 0.7F, 0.8F);
    style_data.pressed.font_color = make_content_color(0.9F, 0.8F, 0.7F, 0.6F);

    const ButtonStyle button_style = build_button_style(style_data);
    DEFN_CHECK_EQ(button_style.font_size, 18);
    DEFN_CHECK_CLOSE(button_style.minimum_size.x, 220.0, 0.001);
    DEFN_CHECK_CLOSE(button_style.minimum_size.y, 44.0, 0.001);
    DEFN_CHECK_EQ(button_style.separation, 9);
    check_color_close(button_style.normal_font, Color(0.1F, 0.2F, 0.3F, 0.4F));
    check_color_close(button_style.hover_font, Color(0.5F, 0.6F, 0.7F, 0.8F));
    check_color_close(button_style.pressed_font, Color(0.9F, 0.8F, 0.7F, 0.6F));
}

DEFN_TEST(menu_style_builds_options_layout_with_defaults_and_overrides) {
    MenuOptionsStyleData options_style;
    options_style.label_font_size = 20;
    options_style.label_min_width = 180;
    options_style.control_min_width = 260;
    options_style.control_min_height = 34;
    options_style.row_separation = 7;
    options_style.section_font_size = 24;
    options_style.value_font_size = 16;
    options_style.section_font_color = make_content_color(1.0F, 0.1F, 0.2F, 0.3F);
    options_style.label_font_color = make_content_color(0.4F, 0.5F, 0.6F, 0.7F);
    options_style.value_font_color = make_content_color(0.8F, 0.9F, 1.0F, 0.5F);

    const OptionsLayout layout = build_options_layout(options_style);
    DEFN_CHECK_EQ(layout.label_font_size, 20);
    DEFN_CHECK_CLOSE(layout.label_minimum_size.x, 180.0, 0.001);
    DEFN_CHECK_CLOSE(layout.control_minimum_size.x, 260.0, 0.001);
    DEFN_CHECK_CLOSE(layout.control_minimum_size.y, 34.0, 0.001);
    DEFN_CHECK_EQ(layout.row_separation, 7);
    DEFN_CHECK_EQ(layout.section_font_size, 24);
    DEFN_CHECK_EQ(layout.value_font_size, 16);
    check_color_close(layout.section_color, Color(1.0F, 0.1F, 0.2F, 0.3F));
    check_color_close(layout.label_color, Color(0.4F, 0.5F, 0.6F, 0.7F));
    check_color_close(layout.value_color, Color(0.8F, 0.9F, 1.0F, 0.5F));
}

DEFN_TEST(menu_style_null_apply_helpers_are_noops) {
    apply_button_theme(nullptr, ButtonStyle{});
    apply_disabled_style(nullptr, true);
    apply_disabled_style(nullptr, false);
}

DEFN_TEST(menu_style_applies_theme_and_disabled_state_to_button) {
    auto *button = memnew(Button);

    ButtonStyle button_style;
    button_style.font_size = 22;
    button_style.normal_font = Color(0.1F, 0.2F, 0.3F, 0.4F);
    button_style.hover_font = Color(0.5F, 0.6F, 0.7F, 0.8F);
    button_style.pressed_font = Color(0.9F, 0.8F, 0.7F, 0.6F);
    button_style.normal_style_data.bg_color = make_content_color(0.2F, 0.2F, 0.2F, 1.0F);
    button_style.hover_style_data.bg_color = make_content_color(0.3F, 0.3F, 0.3F, 1.0F);
    button_style.pressed_style_data.bg_color = make_content_color(0.4F, 0.4F, 0.4F, 1.0F);

    apply_button_theme(button, button_style);
    DEFN_CHECK(button->has_theme_font_size_override("font_size"));
    DEFN_CHECK_EQ(button->get_theme_font_size("font_size"), 22);
    DEFN_CHECK(button->has_theme_stylebox_override("normal"));
    DEFN_CHECK(button->has_theme_stylebox_override("hover"));
    DEFN_CHECK(button->has_theme_stylebox_override("pressed"));
    check_color_close(button->get_theme_color("font_color"), Color(0.1F, 0.2F, 0.3F, 0.4F));
    check_color_close(button->get_theme_color("font_hover_color"), Color(0.5F, 0.6F, 0.7F, 0.8F));
    check_color_close(button->get_theme_color("font_pressed_color"), Color(0.9F, 0.8F, 0.7F, 0.6F));

    apply_button_theme(button, button_style, 16);
    DEFN_CHECK_EQ(button->get_theme_font_size("font_size"), 16);

    apply_disabled_style(button, false);
    DEFN_CHECK(button->is_disabled());
    check_color_close(button->get_modulate(), Color(0.5F, 0.5F, 0.5F, 0.7F));

    apply_disabled_style(button, true);
    DEFN_CHECK(!button->is_disabled());
    check_color_close(button->get_modulate(), Color(1.0F, 1.0F, 1.0F, 1.0F));

    memdelete(button);
}

} // namespace defn