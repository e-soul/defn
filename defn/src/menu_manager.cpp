#include "menu_manager.h"
#include "progression_manager.h"
#include "variant_tools.h"
#include <cmath>
#include <cstdint>
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/check_button.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <utility>

namespace defn {

namespace {

constexpr real_t DEFAULT_ALPHA = 1.0F;

Color parse_color_array(const Array &arr, const Color &fallback = Color(1, 1, 1, 1));
Ref<StyleBoxFlat> make_style(const Dictionary &style_dict);

struct ButtonStyle {
    int font_size = 32;
    Vector2 minimum_size;
    int separation = 16;
    Dictionary normal_style_data;
    Dictionary hover_style_data;
    Dictionary pressed_style_data;
    Color normal_font;
    Color hover_font;
    Color pressed_font;
};

struct OptionsLayout {
    int label_font_size = 24;
    Vector2 label_minimum_size;
    Vector2 control_minimum_size;
    int row_separation = 12;
    int section_font_size = 28;
    int value_font_size = 20;
    Color section_color;
    Color label_color;
    Color value_color;
};

Vector2 make_size(int width, int height) { return {static_cast<real_t>(width), static_cast<real_t>(height)}; }

int32_t to_int32(int64_t value) { return static_cast<int32_t>(value); }

Vector2i parse_resolution_value(const String &value_str, const Vector2i &fallback = Vector2i(1920, 1080)) {
    const PackedStringArray parts = value_str.split("x");
    if (parts.size() != 2) {
        return fallback;
    }

    return {to_int32(parts[0].to_int()), to_int32(parts[1].to_int())};
}

void apply_button_theme(Control *control, const ButtonStyle &button_style, int font_size) {
    control->add_theme_font_size_override("font_size", font_size);
    control->add_theme_stylebox_override("normal", make_style(button_style.normal_style_data));
    control->add_theme_stylebox_override("hover", make_style(button_style.hover_style_data));
    control->add_theme_stylebox_override("pressed", make_style(button_style.pressed_style_data));
    control->add_theme_color_override("font_color", button_style.normal_font);
    control->add_theme_color_override("font_hover_color", button_style.hover_font);
    control->add_theme_color_override("font_pressed_color", button_style.pressed_font);
}

void apply_disabled_style(BaseButton *button, bool enabled) {
    button->set_disabled(!enabled);
    button->set_modulate(enabled ? Color(1, 1, 1, 1) : Color(0.5, 0.5, 0.5, 0.7));
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

void add_section_label(VBoxContainer *button_container, const Dictionary &setting, const OptionsLayout &options_layout) {
    auto *section_label = memnew(Label);
    section_label->set_text(String(setting.get("label", "")));
    section_label->add_theme_font_size_override("font_size", options_layout.section_font_size);
    section_label->add_theme_color_override("font_color", options_layout.section_color);
    section_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    button_container->add_child(section_label);
}

HBoxContainer *create_option_row(const Dictionary &setting, const OptionsLayout &options_layout) {
    auto *row = memnew(HBoxContainer);
    row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    row->add_theme_constant_override("separation", 16);

    auto *name_label = memnew(Label);
    name_label->set_text(String(setting.get("label", "???")));
    name_label->set_custom_minimum_size(options_layout.label_minimum_size);
    name_label->add_theme_font_size_override("font_size", options_layout.label_font_size);
    name_label->add_theme_color_override("font_color", options_layout.label_color);
    name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    row->add_child(name_label);

    return row;
}

bool is_setting(const Dictionary &setting, const String &expected_type, const String &expected_setting_id) {
    return String(setting.get("type", "")) == expected_type && String(setting.get("setting", "")) == expected_setting_id;
}

bool try_add_display_mode_control(MenuManager *manager, HBoxContainer *row, const Dictionary &setting, const ButtonStyle &button_style,
                                  const OptionsLayout &options_layout, DisplayServer::WindowMode current_mode,
                                  std::vector<DisplayServer::WindowMode> &display_mode_values) {
    if (!is_setting(setting, "dropdown", "display_mode")) {
        return false;
    }

    auto *option_button = memnew(OptionButton);
    option_button->set_custom_minimum_size(options_layout.control_minimum_size);
    option_button->set_focus_mode(Control::FOCUS_NONE);
    apply_button_theme(option_button, button_style, options_layout.label_font_size);

    display_mode_values.clear();
    const Array options = setting.get("options", Array());
    int selected_index = 0;
    int option_index = 0;
    for (const Variant &option_variant : options) {
        Dictionary option_data = option_variant;
        option_button->add_item(String(option_data.get("label", "???")));
        const auto mode_value = static_cast<DisplayServer::WindowMode>(VariantTools::as_int(option_data.get("value", 0)));
        display_mode_values.push_back(mode_value);
        if (mode_value == current_mode) {
            selected_index = option_index;
        }
        ++option_index;
    }

    option_button->select(selected_index);
    option_button->connect("item_selected", callable_mp(manager, &MenuManager::on_display_mode_changed));
    row->add_child(option_button);
    return true;
}

bool try_add_resolution_control(MenuManager *manager, HBoxContainer *row, const Dictionary &setting, const ButtonStyle &button_style,
                                const OptionsLayout &options_layout, DisplayServer::WindowMode current_mode, const Vector2i &current_size,
                                OptionButton *&resolution_dropdown, std::vector<Vector2i> &resolution_values) {
    if (!is_setting(setting, "dropdown", "resolution")) {
        return false;
    }

    auto *option_button = memnew(OptionButton);
    option_button->set_custom_minimum_size(options_layout.control_minimum_size);
    option_button->set_focus_mode(Control::FOCUS_NONE);
    apply_button_theme(option_button, button_style, options_layout.label_font_size);

    resolution_values.clear();
    const Array options = setting.get("options", Array());
    int selected_index = 0;
    int option_index = 0;
    for (const Variant &option_variant : options) {
        Dictionary option_data = option_variant;
        option_button->add_item(String(option_data.get("label", "???")));
        const Vector2i resolution = parse_resolution_value(String(option_data.get("value", "")));
        resolution_values.push_back(resolution);
        if (resolution == current_size) {
            selected_index = option_index;
        }
        ++option_index;
    }

    option_button->select(selected_index);

    const bool windowed = current_mode == DisplayServer::WINDOW_MODE_WINDOWED;
    apply_disabled_style(option_button, windowed);

    resolution_dropdown = option_button;
    option_button->connect("item_selected", callable_mp(manager, &MenuManager::on_resolution_changed));
    row->add_child(option_button);
    return true;
}

bool try_add_vsync_control(HBoxContainer *row, const Dictionary &setting, const OptionsLayout &options_layout, bool vsync_on, MenuManager *manager) {
    if (!is_setting(setting, "checkbox", "vsync")) {
        return false;
    }

    auto *check_button = memnew(CheckButton);
    check_button->set_custom_minimum_size(options_layout.control_minimum_size);
    check_button->set_focus_mode(Control::FOCUS_NONE);
    check_button->set_pressed(vsync_on);
    check_button->connect("toggled", callable_mp_static(&MenuManager::on_vsync_toggled));
    row->add_child(check_button);
    return true;
}

bool try_add_volume_control(MenuManager *manager, HBoxContainer *row, const Dictionary &setting, const OptionsLayout &options_layout, AudioServer *audio_server,
                            std::vector<std::pair<String, Label *>> &volume_labels) {
    if (!is_setting(setting, "slider", "bus_volume")) {
        return false;
    }

    const String bus_name = setting.get("bus", "Master");
    const int min_value = VariantTools::as_int(setting.get("min", 0));
    const int max_value = VariantTools::as_int(setting.get("max", 100));
    const int step_value = VariantTools::as_int(setting.get("step", 1));

    const int bus_index = audio_server->get_bus_index(bus_name);
    double current_percent = 100.0;
    if (bus_index >= 0) {
        const auto decibels = audio_server->get_bus_volume_db(bus_index);
        current_percent = std::round(std::pow(10.0, decibels / 20.0) * 100.0);
    }

    auto *slider = memnew(HSlider);
    slider->set_custom_minimum_size(options_layout.control_minimum_size);
    slider->set_min(min_value);
    slider->set_max(max_value);
    slider->set_step(step_value);
    slider->set_value(current_percent);
    slider->set_focus_mode(Control::FOCUS_NONE);
    slider->connect("value_changed", callable_mp(manager, &MenuManager::on_volume_changed).bind(bus_name));
    row->add_child(slider);

    auto *value_label = memnew(Label);
    value_label->set_text(vformat("%d%%", static_cast<int>(current_percent)));
    value_label->set_custom_minimum_size(make_size(60, 0));
    value_label->add_theme_font_size_override("font_size", options_layout.value_font_size);
    value_label->add_theme_color_override("font_color", options_layout.value_color);
    value_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    row->add_child(value_label);

    volume_labels.emplace_back(bus_name, value_label);
    return true;
}

void add_back_button(MenuManager *manager, VBoxContainer *button_container, const Dictionary &back, const ButtonStyle &button_style) {
    if (back.is_empty()) {
        return;
    }

    auto *button = memnew(Button);
    button->set_text(String(back.get("label", "Back")));
    button->set_custom_minimum_size(button_style.minimum_size);
    button->set_focus_mode(Control::FOCUS_NONE);
    apply_button_theme(button, button_style, button_style.font_size);

    const String action = back.get("action", "none");
    const String target = back.get("target", "");
    button->connect("pressed", callable_mp(manager, &MenuManager::on_button_pressed).bind(action, target));
    button_container->add_child(button);
}

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

} // namespace

void MenuManager::_bind_methods() {}

void MenuManager::_ready() {
    if (!load_menu_data()) {
        UtilityFunctions::printerr("MenuManager: Failed to load menu data");
        return;
    }

    load_settings();

    ui_layer_ = memnew(CanvasLayer);
    ui_layer_->set_name("UILayer");
    add_child(ui_layer_);

    setup_background();

    // Total score label (top right)
    auto *progression = ProgressionManager::get_singleton();
    total_score_label_ = memnew(Label);
    total_score_label_->set_text(vformat("Career Score: %d", progression->get_total_score()));
    total_score_label_->set_anchors_preset(Control::PRESET_TOP_RIGHT);
    total_score_label_->set_offset(Side::SIDE_RIGHT, -24.0);
    total_score_label_->set_offset(Side::SIDE_TOP, 16.0);
    total_score_label_->set_offset(Side::SIDE_LEFT, -300.0);
    total_score_label_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    total_score_label_->add_theme_font_size_override("font_size", 24);
    total_score_label_->add_theme_color_override("font_color", Color(1.0, 0.85, 0.3));
    ui_layer_->add_child(total_score_label_);

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
    resolution_dropdown_ = nullptr;
    volume_labels_.clear();
    display_mode_values_.clear();
    resolution_values_.clear();
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

    String menu_type = menu.get("type", "buttons");
    if (menu_type == "options") {
        build_options_ui(menu);
        return;
    }

    Array entries = menu.get("entries", Array());
    Dictionary style_data = menu_data_.get("style", Dictionary());
    const ButtonStyle button_style = build_button_style(style_data);

    button_container_->add_theme_constant_override("separation", button_style.separation);

    for (const Variant &entry_variant : entries) {
        Dictionary entry = entry_variant;
        String label = entry.get("label", "???");
        String action = entry.get("action", "none");
        String target = entry.get("target", "");

        auto *btn = memnew(Button);
        btn->set_text(label);
        btn->set_custom_minimum_size(button_style.minimum_size);
        btn->set_focus_mode(Control::FOCUS_NONE);
        apply_button_theme(btn, button_style, button_style.font_size);

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
    } else if (action == "level_select") {
        show_level_select();
    } else if (action == "start_game") {
        get_tree()->change_scene_to_file("res://scenes/game.tscn");
    } else if (action == "quit") {
        get_tree()->quit();
    }
}

void MenuManager::show_level_select() {
    clear_buttons();
    current_menu_ = "level_select";

    Dictionary style_data = menu_data_.get("style", Dictionary());
    const ButtonStyle button_style = build_button_style(style_data);

    button_container_->add_theme_constant_override("separation", button_style.separation);

    // Title
    auto *title = memnew(Label);
    title->set_text("SELECT LEVEL");
    title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    title->add_theme_font_size_override("font_size", 36);
    title->add_theme_color_override("font_color", Color(1.0, 0.85, 0.3));
    button_container_->add_child(title);

    auto *progression = ProgressionManager::get_singleton();
    const auto &level_data = progression->get_level_unlock_data();

    for (const auto &level : level_data) {
        bool unlocked = progression->is_level_unlocked(level.level_id);
        int best_score = progression->get_highest_level_score(level.level_id);
        bool completed = progression->is_level_completed(level.level_id);

        String label_text = level.level_id.replace("_", " ").capitalize();
        if (!unlocked) {
            label_text += vformat(" (Score: %d needed)", level.score_required);
        } else if (completed) {
            label_text += vformat(" - Best: %d", best_score);
        }

        auto *btn = memnew(Button);
        btn->set_text(label_text);
        btn->set_custom_minimum_size(button_style.minimum_size);
        btn->set_focus_mode(Control::FOCUS_NONE);
        apply_button_theme(btn, button_style, button_style.font_size);

        if (!unlocked) {
            btn->set_disabled(true);
            btn->set_modulate(Color(0.5, 0.5, 0.5, 0.7));
        } else {
            btn->connect("pressed", callable_mp(this, &MenuManager::on_level_selected).bind(level.level_id));
        }

        button_container_->add_child(btn);
    }

    // Back button
    auto *back_btn = memnew(Button);
    back_btn->set_text("Back");
    back_btn->set_custom_minimum_size(button_style.minimum_size);
    back_btn->set_focus_mode(Control::FOCUS_NONE);
    apply_button_theme(back_btn, button_style, button_style.font_size);
    back_btn->connect("pressed", callable_mp(this, &MenuManager::on_button_pressed).bind(String("goto_menu"), String("game_menu")));
    button_container_->add_child(back_btn);
}

void MenuManager::on_level_selected(const String &level_id) {
    auto *progression = ProgressionManager::get_singleton();
    progression->set_current_level_id(level_id);
    get_tree()->change_scene_to_file("res://scenes/game.tscn");
}

void MenuManager::build_options_ui(const Dictionary &menu_def) {
    const Dictionary style = menu_data_.get("style", Dictionary());
    const ButtonStyle button_style = build_button_style(style);
    const OptionsLayout options_layout = build_options_layout(style.get("options", Dictionary()));

    button_container_->add_theme_constant_override("separation", options_layout.row_separation);

    auto *display_server = DisplayServer::get_singleton();
    auto *audio_server = AudioServer::get_singleton();
    const auto current_mode = display_server->window_get_mode();
    const Vector2i current_size = display_server->window_get_size();
    const bool vsync_on = display_server->window_get_vsync_mode() != DisplayServer::VSYNC_DISABLED;

    const Array settings = menu_def.get("settings", Array());
    for (const Variant &setting_variant : settings) {
        Dictionary setting = setting_variant;
        if (String(setting.get("type", "")) == "section") {
            add_section_label(button_container_, setting, options_layout);
            continue;
        }

        auto *row = create_option_row(setting, options_layout);
        const bool handled = try_add_display_mode_control(this, row, setting, button_style, options_layout, current_mode, display_mode_values_) ||
                             try_add_resolution_control(this, row, setting, button_style, options_layout, current_mode, current_size, resolution_dropdown_,
                                                        resolution_values_) ||
                             try_add_vsync_control(row, setting, options_layout, vsync_on, this) ||
                             try_add_volume_control(this, row, setting, options_layout, audio_server, volume_labels_);

        if (handled) {
            button_container_->add_child(row);
        } else {
            row->queue_free();
            UtilityFunctions::printerr("MenuManager: Unknown option setting: ", setting.get("setting", "<missing>"));
        }
    }

    add_back_button(this, button_container_, menu_def.get("back", Dictionary()), button_style);
}

void MenuManager::on_display_mode_changed(int index) {
    if (index < 0 || std::cmp_greater_equal(index, display_mode_values_.size())) {
        return;
    }

    auto *display_server = DisplayServer::get_singleton();
    auto mode = display_mode_values_[index];
    display_server->window_set_mode(mode);

    if (mode == DisplayServer::WINDOW_MODE_WINDOWED) {
        const Vector2i screen_size = display_server->screen_get_size();
        const Vector2i window_size = display_server->window_get_size();
        display_server->window_set_position((screen_size - window_size) / 2);
    }

    const bool windowed = mode == DisplayServer::WINDOW_MODE_WINDOWED;
    if (resolution_dropdown_) {
        apply_disabled_style(resolution_dropdown_, windowed);
    }

    save_settings();
}

void MenuManager::on_resolution_changed(int index) {
    if (index < 0 || std::cmp_greater_equal(index, resolution_values_.size())) {
        return;
    }

    auto *display_server = DisplayServer::get_singleton();
    const Vector2i new_size = resolution_values_[index];
    display_server->window_set_size(new_size);

    const Vector2i screen_size = display_server->screen_get_size();
    display_server->window_set_position((screen_size - new_size) / 2);

    save_settings();
}

void MenuManager::on_vsync_toggled(bool toggled) {
    auto *display_server = DisplayServer::get_singleton();
    display_server->window_set_vsync_mode(toggled ? DisplayServer::VSYNC_ENABLED : DisplayServer::VSYNC_DISABLED);
    save_settings();
}

void MenuManager::on_volume_changed(double value, const String &bus_name) {
    auto *audio = AudioServer::get_singleton();
    int bus_idx = audio->get_bus_index(bus_name);
    if (bus_idx < 0) {
        return;
    }

    const auto linear = static_cast<float>(value / 100.0);
    const auto decibels = (linear > 0.001F) ? 20.0F * std::log10(linear) : -80.0F;
    audio->set_bus_volume_db(bus_idx, decibels);

    for (auto &[name, label] : volume_labels_) {
        if (name == bus_name && label) {
            label->set_text(vformat("%d%%", static_cast<int>(value)));
            break;
        }
    }

    save_settings();
}

void MenuManager::save_settings() {
    auto *display_server = DisplayServer::get_singleton();
    auto *audio_server = AudioServer::get_singleton();

    Ref<ConfigFile> cfg;
    cfg.instantiate();

    cfg->set_value("video", "display_mode", static_cast<int>(display_server->window_get_mode()));

    const Vector2i window_size = display_server->window_get_size();
    cfg->set_value("video", "resolution", vformat("%dx%d", window_size.x, window_size.y));

    cfg->set_value("video", "vsync", display_server->window_get_vsync_mode() != DisplayServer::VSYNC_DISABLED);

    const int master_idx = audio_server->get_bus_index("Master");
    if (master_idx >= 0) {
        const auto decibels = audio_server->get_bus_volume_db(master_idx);
        const double pct = std::round(std::pow(10.0, decibels / 20.0) * 100.0);
        cfg->set_value("audio", "master_volume", pct);
    }

    Error err = cfg->save(SETTINGS_PATH);
    if (err != OK) {
        UtilityFunctions::printerr("MenuManager: Failed to save settings, error: ", err);
    }
}

void MenuManager::load_settings() {
    Ref<ConfigFile> cfg;
    cfg.instantiate();

    if (cfg->load(SETTINGS_PATH) != OK) {
        return; // No saved settings yet — use engine defaults
    }

    auto *display_server = DisplayServer::get_singleton();
    auto *audio_server = AudioServer::get_singleton();

    // Display mode
    if (cfg->has_section_key("video", "display_mode")) {
        auto mode = static_cast<DisplayServer::WindowMode>(VariantTools::as_int(cfg->get_value("video", "display_mode")));
        display_server->window_set_mode(mode);
    }

    // Resolution (only meaningful in windowed mode)
    if (cfg->has_section_key("video", "resolution")) {
        const Vector2i resolution = parse_resolution_value(String(cfg->get_value("video", "resolution")));
        if (display_server->window_get_mode() == DisplayServer::WINDOW_MODE_WINDOWED) {
            display_server->window_set_size(resolution);
            const Vector2i screen_size = display_server->screen_get_size();
            display_server->window_set_position((screen_size - resolution) / 2);
        }
    }

    // VSync
    if (cfg->has_section_key("video", "vsync")) {
        bool vsync = cfg->get_value("video", "vsync");
        display_server->window_set_vsync_mode(vsync ? DisplayServer::VSYNC_ENABLED : DisplayServer::VSYNC_DISABLED);
    }

    // Master volume
    if (cfg->has_section_key("audio", "master_volume")) {
        const double pct = cfg->get_value("audio", "master_volume");
        const auto linear = static_cast<float>(pct / 100.0);
        const auto decibels = (linear > 0.001F) ? 20.0F * std::log10(linear) : -80.0F;
        const int master_idx = audio_server->get_bus_index("Master");
        if (master_idx >= 0) {
            audio_server->set_bus_volume_db(master_idx, decibels);
        }
    }
}

} // namespace defn
