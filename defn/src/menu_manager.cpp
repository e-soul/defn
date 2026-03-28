#include "menu_manager.h"
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

namespace {

Color parse_color_array(const Array &arr, const Color &fallback = Color(1, 1, 1, 1)) {
    if (arr.size() >= 3) {
        const auto r = static_cast<float>(static_cast<double>(arr[0]));
        const auto g = static_cast<float>(static_cast<double>(arr[1]));
        const auto b = static_cast<float>(static_cast<double>(arr[2]));
        const auto a = arr.size() >= 4 ? static_cast<float>(static_cast<double>(arr[3])) : 1.0F;
        return {r, g, b, a};
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

void MenuManager::_bind_methods() {}

void MenuManager::_ready() {
    if (!load_menu_data()) {
        UtilityFunctions::printerr("MenuManager: Failed to load menu data");
        return;
    }

    ui_layer_ = memnew(CanvasLayer);
    ui_layer_->set_name("UILayer");
    add_child(ui_layer_);

    setup_background();

    // Center container spanning the full viewport
    auto *center = memnew(CenterContainer);
    center->set_anchors_preset(Control::PRESET_FULL_RECT);
    center->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    ui_layer_->add_child(center);

    button_container_ = memnew(VBoxContainer);
    button_container_->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    center->add_child(button_container_);

    show_menu("main_menu");
}

bool MenuManager::load_menu_data() {
    const String path = "res://data/menu_data.json";
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (!file.is_valid()) {
        UtilityFunctions::printerr("MenuManager: Failed to open: ", path);
        return false;
    }

    String json_text = file->get_as_text();
    file->close();

    Ref<JSON> json;
    json.instantiate();
    if (json->parse(json_text) != OK) {
        UtilityFunctions::printerr("MenuManager: JSON parse error: ", json->get_error_message());
        return false;
    }

    menu_data_ = json->get_data();
    return true;
}

void MenuManager::setup_background() {
    String bg_path = menu_data_.get("background", "");
    if (bg_path.is_empty()) {
        return;
    }

    auto *loader = ResourceLoader::get_singleton();
    Ref<Texture2D> tex = loader->load(bg_path);
    if (!tex.is_valid()) {
        UtilityFunctions::printerr("MenuManager: Failed to load background: ", bg_path);
        return;
    }

    background_ = memnew(TextureRect);
    background_->set_texture(tex);
    background_->set_anchors_preset(Control::PRESET_FULL_RECT);
    background_->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_COVERED);
    background_->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    ui_layer_->add_child(background_);

    // Background must render behind buttons
    ui_layer_->move_child(background_, 0);
}

void MenuManager::clear_buttons() {
    if (!button_container_) {
        return;
    }
    while (button_container_->get_child_count() > 0) {
        Node *child = button_container_->get_child(0);
        button_container_->remove_child(child);
        child->queue_free();
    }
}

void MenuManager::show_menu(const String &menu_name) {
    clear_buttons();
    current_menu_ = menu_name;

    Dictionary menus = menu_data_.get("menus", Dictionary());
    if (!menus.has(menu_name)) {
        UtilityFunctions::printerr("MenuManager: Unknown menu: ", menu_name);
        return;
    }

    Dictionary menu = menus[menu_name];
    Array entries = menu.get("entries", Array());
    Dictionary style_data = menu_data_.get("style", Dictionary());

    int font_size = static_cast<int>(style_data.get("button_font_size", 32));
    int min_w = static_cast<int>(style_data.get("button_min_width", 400));
    int min_h = static_cast<int>(style_data.get("button_min_height", 60));
    int separation = static_cast<int>(style_data.get("button_separation", 16));

    button_container_->add_theme_constant_override("separation", separation);

    Dictionary normal_style_data = style_data.get("normal", Dictionary());
    Dictionary hover_style_data = style_data.get("hover", Dictionary());
    Dictionary pressed_style_data = style_data.get("pressed", Dictionary());

    Color normal_font = parse_color_array(normal_style_data.get("font_color", Array()), Color(0.9, 0.9, 0.95));
    Color hover_font = parse_color_array(hover_style_data.get("font_color", Array()), Color(1, 1, 1));
    Color pressed_font = parse_color_array(pressed_style_data.get("font_color", Array()), Color(0.8, 0.8, 0.9));

    for (int i = 0; i < entries.size(); ++i) {
        Dictionary entry = entries[i];
        String label = entry.get("label", "???");
        String action = entry.get("action", "none");
        String target = entry.get("target", "");

        auto *btn = memnew(Button);
        btn->set_text(label);
        btn->set_custom_minimum_size(Vector2(static_cast<real_t>(min_w), static_cast<real_t>(min_h)));
        btn->set_focus_mode(Control::FOCUS_NONE);
        btn->add_theme_font_size_override("font_size", font_size);

        // Apply styles
        btn->add_theme_stylebox_override("normal", make_style(normal_style_data));
        btn->add_theme_stylebox_override("hover", make_style(hover_style_data));
        btn->add_theme_stylebox_override("pressed", make_style(pressed_style_data));

        btn->add_theme_color_override("font_color", normal_font);
        btn->add_theme_color_override("font_hover_color", hover_font);
        btn->add_theme_color_override("font_pressed_color", pressed_font);

        if (action == "none") {
            btn->set_disabled(true);
            btn->set_modulate(Color(0.5, 0.5, 0.5, 0.7));
        }

        btn->connect("pressed", callable_mp(this, &MenuManager::on_button_pressed).bind(action, target));
        button_container_->add_child(btn);
    }
}

void MenuManager::on_button_pressed(const String &action, const String &target) {
    if (action == "goto_menu") {
        show_menu(target);
    } else if (action == "start_game") {
        get_tree()->change_scene_to_file("res://scenes/game.tscn");
    } else if (action == "quit") {
        get_tree()->quit();
    }
}

} // namespace defn
