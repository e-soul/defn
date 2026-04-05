#include "menu_manager.h"
#include "progression_manager.h"
#include "variant_tools.h"
#include <cmath>
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

Color parse_color_array(const Array &arr, const Color &fallback = Color(1, 1, 1, 1)) {
    if (arr.size() >= 3) {
        const auto r = VariantTools::as_real(arr[0]);
        const auto g = VariantTools::as_real(arr[1]);
        const auto b = VariantTools::as_real(arr[2]);
        const auto a = arr.size() >= 4 ? VariantTools::as_real(arr[3]) : 1.0;
        return Color(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a));
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

    int font_size = VariantTools::as_int(style_data.get("button_font_size", 32));
    int min_w = VariantTools::as_int(style_data.get("button_min_width", 400));
    int min_h = VariantTools::as_int(style_data.get("button_min_height", 60));
    int separation = VariantTools::as_int(style_data.get("button_separation", 16));

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
        btn->set_custom_minimum_size(Vector2(min_w, min_h));
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
    int font_size = VariantTools::as_int(style_data.get("button_font_size", 32));
    int min_w = VariantTools::as_int(style_data.get("button_min_width", 400));
    int min_h = VariantTools::as_int(style_data.get("button_min_height", 60));
    int separation = VariantTools::as_int(style_data.get("button_separation", 16));

    Dictionary normal_style_data = style_data.get("normal", Dictionary());
    Dictionary hover_style_data = style_data.get("hover", Dictionary());
    Dictionary pressed_style_data = style_data.get("pressed", Dictionary());

    Color normal_font = parse_color_array(normal_style_data.get("font_color", Array()), Color(0.9, 0.9, 0.95));
    Color hover_font = parse_color_array(hover_style_data.get("font_color", Array()), Color(1, 1, 1));
    Color pressed_font = parse_color_array(pressed_style_data.get("font_color", Array()), Color(0.8, 0.8, 0.9));

    button_container_->add_theme_constant_override("separation", separation);

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
        btn->set_custom_minimum_size(Vector2(min_w, min_h));
        btn->set_focus_mode(Control::FOCUS_NONE);
        btn->add_theme_font_size_override("font_size", font_size);

        btn->add_theme_stylebox_override("normal", make_style(normal_style_data));
        btn->add_theme_stylebox_override("hover", make_style(hover_style_data));
        btn->add_theme_stylebox_override("pressed", make_style(pressed_style_data));

        btn->add_theme_color_override("font_color", normal_font);
        btn->add_theme_color_override("font_hover_color", hover_font);
        btn->add_theme_color_override("font_pressed_color", pressed_font);

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
    back_btn->set_custom_minimum_size(Vector2(min_w, min_h));
    back_btn->set_focus_mode(Control::FOCUS_NONE);
    back_btn->add_theme_font_size_override("font_size", font_size);
    back_btn->add_theme_stylebox_override("normal", make_style(normal_style_data));
    back_btn->add_theme_stylebox_override("hover", make_style(hover_style_data));
    back_btn->add_theme_stylebox_override("pressed", make_style(pressed_style_data));
    back_btn->add_theme_color_override("font_color", normal_font);
    back_btn->add_theme_color_override("font_hover_color", hover_font);
    back_btn->add_theme_color_override("font_pressed_color", pressed_font);
    back_btn->connect("pressed", callable_mp(this, &MenuManager::on_button_pressed).bind(String("goto_menu"), String("game_menu")));
    button_container_->add_child(back_btn);
}

void MenuManager::on_level_selected(const String &level_id) {
    auto *progression = ProgressionManager::get_singleton();
    progression->set_current_level_id(level_id);
    get_tree()->change_scene_to_file("res://scenes/game.tscn");
}

void MenuManager::build_options_ui(const Dictionary &menu_def) {
    Dictionary style = menu_data_.get("style", Dictionary());
    Dictionary opts_style = style.get("options", Dictionary());
    Dictionary normal_sd = style.get("normal", Dictionary());
    Dictionary hover_sd = style.get("hover", Dictionary());
    Dictionary pressed_sd = style.get("pressed", Dictionary());

    int label_font_size = VariantTools::as_int(opts_style.get("label_font_size", 24));
    int label_min_w = VariantTools::as_int(opts_style.get("label_min_width", 250));
    int control_min_w = VariantTools::as_int(opts_style.get("control_min_width", 300));
    int control_min_h = VariantTools::as_int(opts_style.get("control_min_height", 40));
    int row_sep = VariantTools::as_int(opts_style.get("row_separation", 12));
    int section_font_size = VariantTools::as_int(opts_style.get("section_font_size", 28));
    int value_font_size = VariantTools::as_int(opts_style.get("value_font_size", 20));
    Color section_color = parse_color_array(opts_style.get("section_font_color", Array()), Color(1, 0.85, 0.3));
    Color label_color = parse_color_array(opts_style.get("label_font_color", Array()), Color(0.85, 0.85, 0.9));
    Color value_color = parse_color_array(opts_style.get("value_font_color", Array()), Color(0.7, 0.8, 1.0));
    Color normal_font = parse_color_array(normal_sd.get("font_color", Array()), Color(0.9, 0.9, 0.95));
    Color hover_font = parse_color_array(hover_sd.get("font_color", Array()), Color(1, 1, 1));
    Color pressed_font = parse_color_array(pressed_sd.get("font_color", Array()), Color(0.8, 0.8, 0.9));

    button_container_->add_theme_constant_override("separation", row_sep);

    // Read current engine state
    auto *ds = DisplayServer::get_singleton();
    auto *audio = AudioServer::get_singleton();
    auto current_mode = ds->window_get_mode();
    Vector2i current_size = ds->window_get_size();
    bool vsync_on = ds->window_get_vsync_mode() != DisplayServer::VSYNC_DISABLED;

    Array settings = menu_def.get("settings", Array());
    for (int i = 0; i < settings.size(); ++i) {
        Dictionary setting = settings[i];
        String type = setting.get("type", "");

        if (type == "section") {
            auto *section_label = memnew(Label);
            section_label->set_text(String(setting.get("label", "")));
            section_label->add_theme_font_size_override("font_size", section_font_size);
            section_label->add_theme_color_override("font_color", section_color);
            section_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
            button_container_->add_child(section_label);
            continue;
        }

        auto *row = memnew(HBoxContainer);
        row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
        row->add_theme_constant_override("separation", 16);

        auto *name_label = memnew(Label);
        name_label->set_text(String(setting.get("label", "???")));
        name_label->set_custom_minimum_size(Vector2(label_min_w, 0));
        name_label->add_theme_font_size_override("font_size", label_font_size);
        name_label->add_theme_color_override("font_color", label_color);
        name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
        row->add_child(name_label);

        String setting_id = setting.get("setting", "");

        if (type == "dropdown" && setting_id == "display_mode") {
            auto *opt = memnew(OptionButton);
            opt->set_custom_minimum_size(Vector2(control_min_w, control_min_h));
            opt->set_focus_mode(Control::FOCUS_NONE);
            opt->add_theme_font_size_override("font_size", label_font_size);
            opt->add_theme_stylebox_override("normal", make_style(normal_sd));
            opt->add_theme_stylebox_override("hover", make_style(hover_sd));
            opt->add_theme_stylebox_override("pressed", make_style(pressed_sd));
            opt->add_theme_color_override("font_color", normal_font);
            opt->add_theme_color_override("font_hover_color", hover_font);
            opt->add_theme_color_override("font_pressed_color", pressed_font);

            display_mode_values_.clear();
            Array options = setting.get("options", Array());
            int selected = 0;
            for (int j = 0; j < options.size(); ++j) {
                Dictionary opt_data = options[j];
                opt->add_item(String(opt_data.get("label", "???")));
                auto mode_val = static_cast<DisplayServer::WindowMode>(VariantTools::as_int(opt_data.get("value", 0)));
                display_mode_values_.push_back(mode_val);
                if (mode_val == current_mode) {
                    selected = j;
                }
            }
            opt->select(selected);
            opt->connect("item_selected", callable_mp(this, &MenuManager::on_display_mode_changed));
            row->add_child(opt);
        } else if (type == "dropdown" && setting_id == "resolution") {
            auto *opt = memnew(OptionButton);
            opt->set_custom_minimum_size(Vector2(control_min_w, control_min_h));
            opt->set_focus_mode(Control::FOCUS_NONE);
            opt->add_theme_font_size_override("font_size", label_font_size);
            opt->add_theme_stylebox_override("normal", make_style(normal_sd));
            opt->add_theme_stylebox_override("hover", make_style(hover_sd));
            opt->add_theme_stylebox_override("pressed", make_style(pressed_sd));
            opt->add_theme_color_override("font_color", normal_font);
            opt->add_theme_color_override("font_hover_color", hover_font);
            opt->add_theme_color_override("font_pressed_color", pressed_font);

            resolution_values_.clear();
            Array options = setting.get("options", Array());
            int selected = 0;
            for (int j = 0; j < options.size(); ++j) {
                Dictionary opt_data = options[j];
                opt->add_item(String(opt_data.get("label", "???")));
                String value_str = opt_data.get("value", "");
                PackedStringArray parts = value_str.split("x");
                Vector2i res(1920, 1080);
                if (parts.size() == 2) {
                    res = Vector2i(parts[0].to_int(), parts[1].to_int());
                }
                resolution_values_.push_back(res);
                if (res == current_size) {
                    selected = j;
                }
            }
            opt->select(selected);

            bool windowed = (current_mode == DisplayServer::WINDOW_MODE_WINDOWED);
            opt->set_disabled(!windowed);
            if (!windowed) {
                opt->set_modulate(Color(0.5, 0.5, 0.5, 0.7));
            }

            resolution_dropdown_ = opt;
            opt->connect("item_selected", callable_mp(this, &MenuManager::on_resolution_changed));
            row->add_child(opt);
        } else if (type == "checkbox" && setting_id == "vsync") {
            auto *check = memnew(CheckButton);
            check->set_custom_minimum_size(Vector2(control_min_w, control_min_h));
            check->set_focus_mode(Control::FOCUS_NONE);
            check->set_pressed(vsync_on);
            check->connect("toggled", callable_mp(this, &MenuManager::on_vsync_toggled));
            row->add_child(check);
        } else if (type == "slider" && setting_id == "bus_volume") {
            String bus_name = setting.get("bus", "Master");
            int min_val = VariantTools::as_int(setting.get("min", 0));
            int max_val = VariantTools::as_int(setting.get("max", 100));
            int step = VariantTools::as_int(setting.get("step", 1));

            int bus_idx = audio->get_bus_index(bus_name);
            double current_pct = 100.0;
            if (bus_idx >= 0) {
                const float db = audio->get_bus_volume_db(bus_idx);
                current_pct = std::round(std::pow(10.0, db / 20.0) * 100.0);
            }

            auto *slider = memnew(HSlider);
            slider->set_custom_minimum_size(Vector2(control_min_w, control_min_h));
            slider->set_min(min_val);
            slider->set_max(max_val);
            slider->set_step(step);
            slider->set_value(current_pct);
            slider->set_focus_mode(Control::FOCUS_NONE);
            slider->connect("value_changed", callable_mp(this, &MenuManager::on_volume_changed).bind(bus_name));
            row->add_child(slider);

            auto *val_label = memnew(Label);
            val_label->set_text(vformat("%d%%", static_cast<int>(current_pct)));
            val_label->set_custom_minimum_size(Vector2(60, 0));
            val_label->add_theme_font_size_override("font_size", value_font_size);
            val_label->add_theme_color_override("font_color", value_color);
            val_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
            row->add_child(val_label);

            volume_labels_.emplace_back(bus_name, val_label);
        }

        button_container_->add_child(row);
    }

    // Back button
    Dictionary back = menu_def.get("back", Dictionary());
    if (!back.is_empty()) {
        int btn_font_size = VariantTools::as_int(style.get("button_font_size", 32));
        int btn_min_w = VariantTools::as_int(style.get("button_min_width", 400));
        int btn_min_h = VariantTools::as_int(style.get("button_min_height", 60));

        auto *btn = memnew(Button);
        btn->set_text(String(back.get("label", "Back")));
        btn->set_custom_minimum_size(Vector2(btn_min_w, btn_min_h));
        btn->set_focus_mode(Control::FOCUS_NONE);
        btn->add_theme_font_size_override("font_size", btn_font_size);
        btn->add_theme_stylebox_override("normal", make_style(normal_sd));
        btn->add_theme_stylebox_override("hover", make_style(hover_sd));
        btn->add_theme_stylebox_override("pressed", make_style(pressed_sd));
        btn->add_theme_color_override("font_color", normal_font);
        btn->add_theme_color_override("font_hover_color", hover_font);
        btn->add_theme_color_override("font_pressed_color", pressed_font);

        String action = back.get("action", "none");
        String target = back.get("target", "");
        btn->connect("pressed", callable_mp(this, &MenuManager::on_button_pressed).bind(action, target));
        button_container_->add_child(btn);
    }
}

void MenuManager::on_display_mode_changed(int index) {
    if (index < 0 || std::cmp_greater_equal(index, display_mode_values_.size())) {
        return;
    }

    auto *ds = DisplayServer::get_singleton();
    auto mode = display_mode_values_[index];
    ds->window_set_mode(mode);

    if (mode == DisplayServer::WINDOW_MODE_WINDOWED) {
        Vector2i screen_size = ds->screen_get_size();
        Vector2i win_size = ds->window_get_size();
        ds->window_set_position((screen_size - win_size) / 2);
    }

    bool windowed = (mode == DisplayServer::WINDOW_MODE_WINDOWED);
    if (resolution_dropdown_) {
        resolution_dropdown_->set_disabled(!windowed);
        resolution_dropdown_->set_modulate(windowed ? Color(1, 1, 1, 1) : Color(0.5, 0.5, 0.5, 0.7));
    }

    save_settings();
}

void MenuManager::on_resolution_changed(int index) {
    if (index < 0 || std::cmp_greater_equal(index, resolution_values_.size())) {
        return;
    }

    auto *ds = DisplayServer::get_singleton();
    Vector2i new_size = resolution_values_[index];
    ds->window_set_size(new_size);

    Vector2i screen_size = ds->screen_get_size();
    ds->window_set_position((screen_size - new_size) / 2);

    save_settings();
}

void MenuManager::on_vsync_toggled(bool toggled) {
    auto *ds = DisplayServer::get_singleton();
    ds->window_set_vsync_mode(toggled ? DisplayServer::VSYNC_ENABLED : DisplayServer::VSYNC_DISABLED);
    save_settings();
}

void MenuManager::on_volume_changed(double value, const String &bus_name) {
    auto *audio = AudioServer::get_singleton();
    int bus_idx = audio->get_bus_index(bus_name);
    if (bus_idx < 0) {
        return;
    }

    const float linear = static_cast<float>(value / 100.0);
    const float db = (linear > 0.001F) ? 20.0F * std::log10(linear) : -80.0F;
    audio->set_bus_volume_db(bus_idx, db);

    for (auto &[name, label] : volume_labels_) {
        if (name == bus_name && label) {
            label->set_text(vformat("%d%%", static_cast<int>(value)));
            break;
        }
    }

    save_settings();
}

void MenuManager::save_settings() {
    auto *ds = DisplayServer::get_singleton();
    auto *audio = AudioServer::get_singleton();

    Ref<ConfigFile> cfg;
    cfg.instantiate();

    cfg->set_value("video", "display_mode", static_cast<int>(ds->window_get_mode()));

    Vector2i win_size = ds->window_get_size();
    cfg->set_value("video", "resolution", vformat("%dx%d", win_size.x, win_size.y));

    cfg->set_value("video", "vsync", ds->window_get_vsync_mode() != DisplayServer::VSYNC_DISABLED);

    int master_idx = audio->get_bus_index("Master");
    if (master_idx >= 0) {
        const float db = audio->get_bus_volume_db(master_idx);
        const double pct = std::round(std::pow(10.0, db / 20.0) * 100.0);
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

    auto *ds = DisplayServer::get_singleton();
    auto *audio = AudioServer::get_singleton();

    // Display mode
    if (cfg->has_section_key("video", "display_mode")) {
        auto mode = static_cast<DisplayServer::WindowMode>(VariantTools::as_int(cfg->get_value("video", "display_mode")));
        ds->window_set_mode(mode);
    }

    // Resolution (only meaningful in windowed mode)
    if (cfg->has_section_key("video", "resolution")) {
        String res_str = cfg->get_value("video", "resolution");
        PackedStringArray parts = res_str.split("x");
        if (parts.size() == 2) {
            Vector2i res(parts[0].to_int(), parts[1].to_int());
            if (ds->window_get_mode() == DisplayServer::WINDOW_MODE_WINDOWED) {
                ds->window_set_size(res);
                Vector2i screen_size = ds->screen_get_size();
                ds->window_set_position((screen_size - res) / 2);
            }
        }
    }

    // VSync
    if (cfg->has_section_key("video", "vsync")) {
        bool vsync = cfg->get_value("video", "vsync");
        ds->window_set_vsync_mode(vsync ? DisplayServer::VSYNC_ENABLED : DisplayServer::VSYNC_DISABLED);
    }

    // Master volume
    if (cfg->has_section_key("audio", "master_volume")) {
        const double pct = cfg->get_value("audio", "master_volume");
        const float linear = static_cast<float>(pct / 100.0);
        const float db = (linear > 0.001F) ? 20.0F * std::log10(linear) : -80.0F;
        int master_idx = audio->get_bus_index("Master");
        if (master_idx >= 0) {
            audio->set_bus_volume_db(master_idx, db);
        }
    }
}

} // namespace defn
