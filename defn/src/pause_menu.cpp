#include "pause_menu.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

Color parse_color_array(const Array &arr, const Color &fallback = Color(1, 1, 1, 1)) {
    if (arr.size() >= 3) {
        const auto red = static_cast<float>(static_cast<double>(arr[0]));
        const auto green = static_cast<float>(static_cast<double>(arr[1]));
        const auto blue = static_cast<float>(static_cast<double>(arr[2]));
        const auto alpha = arr.size() >= 4 ? static_cast<float>(static_cast<double>(arr[3])) : 1.0F;
        return {red, green, blue, alpha};
    }
    return fallback;
}

Ref<StyleBoxFlat> make_style(const Dictionary &style_dict) {
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(parse_color_array(style_dict.get("bg_color", Array())));
    style->set_border_color(parse_color_array(style_dict.get("border_color", Array()), Color(0.4, 0.4, 0.5)));
    style->set_border_width_all(static_cast<int>(style_dict.get("border_width", 2)));
    style->set_corner_radius_all(static_cast<int>(style_dict.get("corner_radius", 8)));
    return style;
}

} // namespace

void PauseMenu::_bind_methods() {}

void PauseMenu::_ready() {
    // Render on top of everything
    set_layer(100);
    // Keep processing input while tree is paused
    set_process_mode(Node::PROCESS_MODE_ALWAYS);

    if (!load_config()) {
        UtilityFunctions::printerr("PauseMenu: Failed to load config");
        return;
    }

    build_ui();
    set_paused(false);
}

void PauseMenu::_input(const Ref<InputEvent> &event) {
    auto *key = Object::cast_to<InputEventKey>(event.ptr());
    if (key && key->is_pressed() && !key->is_echo() && key->get_keycode() == KEY_ESCAPE) {
        toggle_pause();
        get_viewport()->set_input_as_handled();
    }
}

bool PauseMenu::load_config() {
    const String path = "res://data/menu_data.json";
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (!file.is_valid()) {
        return false;
    }

    String json_text = file->get_as_text();
    file->close();

    Ref<JSON> json;
    json.instantiate();
    if (json->parse(json_text) != OK) {
        return false;
    }

    menu_data_ = json->get_data();
    return true;
}

void PauseMenu::build_ui() {
    Dictionary menus = menu_data_.get("menus", Dictionary());
    Dictionary pause_def = menus.get("pause_menu", Dictionary());
    Dictionary style = menu_data_.get("style", Dictionary());

    // Dark overlay
    Color overlay_color = parse_color_array(pause_def.get("overlay_color", Array()), Color(0, 0, 0, 0.6));
    overlay_ = memnew(ColorRect);
    overlay_->set_anchors_preset(Control::PRESET_FULL_RECT);
    overlay_->set_color(overlay_color);
    overlay_->set_mouse_filter(Control::MOUSE_FILTER_STOP);
    add_child(overlay_);

    // Center container
    auto *center = memnew(CenterContainer);
    center->set_anchors_preset(Control::PRESET_FULL_RECT);
    center->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    add_child(center);

    button_container_ = memnew(VBoxContainer);
    button_container_->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    center->add_child(button_container_);

    int font_size = static_cast<int>(style.get("button_font_size", 32));
    int min_w = static_cast<int>(style.get("button_min_width", 400));
    int min_h = static_cast<int>(style.get("button_min_height", 60));
    int separation = static_cast<int>(style.get("button_separation", 16));

    button_container_->add_theme_constant_override("separation", separation);

    Dictionary normal_sd = style.get("normal", Dictionary());
    Dictionary hover_sd = style.get("hover", Dictionary());
    Dictionary pressed_sd = style.get("pressed", Dictionary());

    Color normal_font = parse_color_array(normal_sd.get("font_color", Array()), Color(0.9, 0.9, 0.95));
    Color hover_font = parse_color_array(hover_sd.get("font_color", Array()), Color(1, 1, 1));
    Color pressed_font = parse_color_array(pressed_sd.get("font_color", Array()), Color(0.8, 0.8, 0.9));

    Array entries = pause_def.get("entries", Array());
    for (const auto &entry_variant : entries) {
        Dictionary entry = entry_variant;
        String label = entry.get("label", "???");
        String action = entry.get("action", "");

        auto *btn = memnew(Button);
        btn->set_text(label);
        btn->set_custom_minimum_size(Vector2(static_cast<real_t>(min_w), static_cast<real_t>(min_h)));
        btn->set_focus_mode(Control::FOCUS_NONE);
        btn->add_theme_font_size_override("font_size", font_size);

        btn->add_theme_stylebox_override("normal", make_style(normal_sd));
        btn->add_theme_stylebox_override("hover", make_style(hover_sd));
        btn->add_theme_stylebox_override("pressed", make_style(pressed_sd));
        btn->add_theme_color_override("font_color", normal_font);
        btn->add_theme_color_override("font_hover_color", hover_font);
        btn->add_theme_color_override("font_pressed_color", pressed_font);

        if (action == "resume") {
            btn->connect("pressed", callable_mp(this, &PauseMenu::on_resume));
        } else if (action == "main_menu") {
            btn->connect("pressed", callable_mp(this, &PauseMenu::on_main_menu));
        }

        button_container_->add_child(btn);
    }
}

void PauseMenu::toggle_pause() { set_paused(!paused_); }

void PauseMenu::set_paused(bool paused) {
    paused_ = paused;
    get_tree()->set_pause(paused_);

    if (overlay_) {
        overlay_->set_visible(paused_);
    }
    if (button_container_ && button_container_->get_parent()) {
        auto *center = Object::cast_to<Node>(button_container_->get_parent());
        center->set_process_mode(paused_ ? PROCESS_MODE_ALWAYS : PROCESS_MODE_DISABLED);
        button_container_->set_visible(paused_);
    }
}

void PauseMenu::on_resume() { set_paused(false); }

void PauseMenu::on_main_menu() {
    set_paused(false);
    get_tree()->change_scene_to_file("res://scenes/menu.tscn");
}

} // namespace defn
