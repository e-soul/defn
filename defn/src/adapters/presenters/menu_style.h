#ifndef MENU_STYLE_H
#define MENU_STYLE_H

#include "menu_models.h"

#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace defn {

using namespace godot;

struct ButtonStyle {
    int font_size = 32;
    Vector2 minimum_size;
    int separation = 16;
    MenuStyleBoxData normal_style_data;
    MenuStyleBoxData hover_style_data;
    MenuStyleBoxData pressed_style_data;
    godot::Color normal_font;
    godot::Color hover_font;
    godot::Color pressed_font;
};

struct OptionsLayout {
    int label_font_size = 24;
    Vector2 label_minimum_size;
    Vector2 control_minimum_size;
    int row_separation = 12;
    int section_font_size = 28;
    int value_font_size = 20;
    godot::Color section_color;
    godot::Color label_color;
    godot::Color value_color;
};

Vector2 make_size(int width, int height);
godot::Color parse_color_array(const Array &arr, const godot::Color &fallback = godot::Color(1, 1, 1, 1));
Ref<StyleBoxFlat> make_style(const MenuStyleBoxData &style_data);
ButtonStyle build_button_style(const MenuStyleData &style_data);
OptionsLayout build_options_layout(const MenuOptionsStyleData &options_style);
void apply_button_theme(Control *control, const ButtonStyle &button_style, int font_size = -1);
void apply_disabled_style(BaseButton *button, bool enabled);

} // namespace defn

#endif