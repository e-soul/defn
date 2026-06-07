#include "menu_manager.h"
#include "data_paths.h"
#include "menu_data_loader.h"
#include "menu_style.h"
#include "owned_upgrades_panel.h"
#include "progression_manager.h"
#include "progression_presentation.h"
#include "scene_navigator.h"
#include "settings_service.h"
#include "variant_tools.h"
#include <cmath>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/check_button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <utility>

namespace defn {

namespace {

constexpr real_t PROGRESSION_SCREEN_WIDTH_RATIO = 0.58;
constexpr real_t PROGRESSION_SCREEN_HEIGHT_RATIO = 0.6;

Vector2 get_progression_screen_size(Node *parent) {
    if (parent == nullptr || parent->get_viewport() == nullptr) {
        return make_size(800, 360);
    }

    const Vector2 viewport_size = parent->get_viewport()->get_visible_rect().size;
    if (viewport_size.x <= 0.0 || viewport_size.y <= 0.0) {
        return make_size(800, 360);
    }

    return {viewport_size.x * PROGRESSION_SCREEN_WIDTH_RATIO, viewport_size.y * PROGRESSION_SCREEN_HEIGHT_RATIO};
}

void add_section_label(VBoxContainer *button_container, const MenuSetting &setting, const OptionsLayout &options_layout) {
    auto *section_label = memnew(Label);
    section_label->set_text(setting.label);
    section_label->add_theme_font_size_override("font_size", options_layout.section_font_size);
    section_label->add_theme_color_override("font_color", options_layout.section_color);
    section_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    button_container->add_child(section_label);
}

HBoxContainer *create_option_row(const MenuSetting &setting, const OptionsLayout &options_layout) {
    auto *row = memnew(HBoxContainer);
    row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    row->add_theme_constant_override("separation", 16);

    auto *name_label = memnew(Label);
    name_label->set_text(setting.label.is_empty() ? String("???") : setting.label);
    name_label->set_custom_minimum_size(options_layout.label_minimum_size);
    name_label->add_theme_font_size_override("font_size", options_layout.label_font_size);
    name_label->add_theme_color_override("font_color", options_layout.label_color);
    name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    row->add_child(name_label);

    return row;
}

bool try_add_display_mode_control(MenuManager *manager, HBoxContainer *row, const MenuSetting &setting, const ButtonStyle &button_style,
                                  const OptionsLayout &options_layout, DisplayServer::WindowMode current_mode,
                                  std::vector<DisplayServer::WindowMode> &display_mode_values) {
    if (setting.kind != MenuSettingKind::DISPLAY_MODE) {
        return false;
    }

    auto *option_button = memnew(OptionButton);
    option_button->set_custom_minimum_size(options_layout.control_minimum_size);
    option_button->set_focus_mode(Control::FOCUS_NONE);
    apply_button_theme(option_button, button_style, options_layout.label_font_size);

    display_mode_values.clear();
    int selected_index = 0;
    int option_index = 0;
    for (const auto &option : setting.options) {
        option_button->add_item(option.label);
        const auto mode_value = static_cast<DisplayServer::WindowMode>(option.value.to_int());
        display_mode_values.push_back(mode_value);
        if (static_cast<int>(mode_value) == static_cast<int>(current_mode)) {
            selected_index = option_index;
        }
        ++option_index;
    }

    option_button->select(selected_index);
    option_button->connect("item_selected", callable_mp(manager, &MenuManager::on_display_mode_changed));
    row->add_child(option_button);
    return true;
}

bool try_add_resolution_control(MenuManager *manager, HBoxContainer *row, const MenuSetting &setting, const ButtonStyle &button_style,
                                const OptionsLayout &options_layout, DisplayServer::WindowMode current_mode, const Vector2i &current_size,
                                OptionButton *&resolution_dropdown, std::vector<Vector2i> &resolution_values) {
    if (setting.kind != MenuSettingKind::RESOLUTION) {
        return false;
    }

    auto *option_button = memnew(OptionButton);
    option_button->set_custom_minimum_size(options_layout.control_minimum_size);
    option_button->set_focus_mode(Control::FOCUS_NONE);
    apply_button_theme(option_button, button_style, options_layout.label_font_size);

    resolution_values.clear();
    int selected_index = 0;
    int option_index = 0;
    for (const auto &option : setting.options) {
        option_button->add_item(option.label);
        const Vector2i resolution = SettingsService::parse_resolution_value(option.value);
        resolution_values.push_back(resolution);
        if (resolution == current_size) {
            selected_index = option_index;
        }
        ++option_index;
    }

    option_button->select(selected_index);

    const bool windowed = static_cast<int>(current_mode) == static_cast<int>(DisplayServer::WINDOW_MODE_WINDOWED);
    apply_disabled_style(option_button, windowed);

    resolution_dropdown = option_button;
    option_button->connect("item_selected", callable_mp(manager, &MenuManager::on_resolution_changed));
    row->add_child(option_button);
    return true;
}

bool try_add_vsync_control(HBoxContainer *row, const MenuSetting &setting, const OptionsLayout &options_layout, bool vsync_on, MenuManager *manager) {
    if (setting.kind != MenuSettingKind::VSYNC) {
        return false;
    }

    auto *check_button = memnew(CheckButton);
    check_button->set_custom_minimum_size(options_layout.control_minimum_size);
    check_button->set_focus_mode(Control::FOCUS_NONE);
    check_button->set_pressed(vsync_on);
    check_button->connect("toggled", callable_mp(manager, &MenuManager::on_vsync_toggled));
    row->add_child(check_button);
    return true;
}

bool try_add_volume_control(MenuManager *manager, HBoxContainer *row, const MenuSetting &setting, const OptionsLayout &options_layout,
                            const SettingsState &settings_state, std::vector<std::pair<String, Label *>> &volume_labels) {
    if (setting.kind != MenuSettingKind::BUS_VOLUME) {
        return false;
    }

    const String bus_name = setting.bus_name.is_empty() ? String("Master") : setting.bus_name;
    const int min_value = setting.min_value;
    const int max_value = setting.max_value;
    const int step_value = setting.step_value;
    const double current_percent = SettingsService::get_bus_volume_percent(settings_state, bus_name);

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

void add_back_button(MenuManager *manager, VBoxContainer *button_container, const std::optional<MenuAction> &back, const ButtonStyle &button_style) {
    if (!back.has_value()) {
        return;
    }

    auto *button = memnew(Button);
    button->set_text(back->label.is_empty() ? String("Back") : back->label);
    button->set_custom_minimum_size(button_style.minimum_size);
    button->set_focus_mode(Control::FOCUS_NONE);
    apply_button_theme(button, button_style, button_style.font_size);

    button->connect("pressed", callable_mp(manager, &MenuManager::on_button_pressed).bind(back->action, back->target));
    button_container->add_child(button);
}

} // namespace

void MenuManager::_bind_methods() {}

void MenuManager::_ready() {
    if (!load_menu_data()) {
        UtilityFunctions::printerr("MenuManager: Failed to load menu data");
        return;
    }

    settings_state_ = SettingsService::load_or_default();
    SettingsService::apply(settings_state_);

    ui_layer_ = memnew(CanvasLayer);
    ui_layer_->set_name("UILayer");
    add_child(ui_layer_);

    setup_background();

    // Total score label (top right)
    auto *progression = CampaignService::get_singleton();
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
    const auto loaded_menu_data = MenuDataLoader::load(DataPaths::MENU_DATA);
    if (!loaded_menu_data) {
        return false;
    }

    menu_data_ = *loaded_menu_data;
    return true;
}

void MenuManager::setup_background() {
    const String bg_path = menu_data_.background;
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

    const MenuDefinition *menu = menu_data_.find_menu(menu_name);
    if (menu == nullptr) {
        UtilityFunctions::printerr("MenuManager: Unknown menu: ", menu_name);
        return;
    }

    if (menu->type == MenuDefinitionType::OPTIONS) {
        build_options_ui(*menu);
        return;
    }

    const ButtonStyle button_style = build_button_style(menu_data_.style_data);

    button_container_->add_theme_constant_override("separation", button_style.separation);

    for (const auto &entry : menu->entries) {
        auto *btn = memnew(Button);
        btn->set_text(entry.label.is_empty() ? String("???") : entry.label);
        btn->set_custom_minimum_size(button_style.minimum_size);
        btn->set_focus_mode(Control::FOCUS_NONE);
        apply_button_theme(btn, button_style, button_style.font_size);

        if (entry.action == "none") {
            btn->set_disabled(true);
            btn->set_modulate(Color(0.5, 0.5, 0.5, 0.7));
        }

        btn->connect("pressed", callable_mp(this, &MenuManager::on_button_pressed).bind(entry.action, entry.target));
        button_container_->add_child(btn);
    }
}

void MenuManager::on_button_pressed(const String &action, const String &target) {
    if (action == "goto_menu") {
        show_menu(target);
    } else if (action == "level_select") {
        show_level_select();
    } else if (action == "progression") {
        show_progression();
    } else if (action == "start_game") {
        SceneNavigator::go_to_current_level(get_tree());
    } else if (action == "quit") {
        SceneNavigator::quit(get_tree());
    }
}

void MenuManager::show_level_select() {
    clear_buttons();
    current_menu_ = "level_select";

    const ButtonStyle button_style = build_button_style(menu_data_.style_data);

    button_container_->add_theme_constant_override("separation", button_style.separation);

    // Title
    auto *title = memnew(Label);
    title->set_text("SELECT LEVEL");
    title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    title->add_theme_font_size_override("font_size", 36);
    title->add_theme_color_override("font_color", Color(1.0, 0.85, 0.3));
    button_container_->add_child(title);

    auto *progression = CampaignService::get_singleton();
    const auto &level_data = progression->get_level_unlock_data();

    for (const auto &level : level_data) {
        bool unlocked = progression->is_level_unlocked(level.level_id);

        auto *btn = memnew(Button);
        btn->set_text(ProgressionPresentation::format_level_button_label(*progression, level));
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

void MenuManager::on_level_selected(const String &level_id) { SceneNavigator::go_to_level(get_tree(), level_id); }

void MenuManager::show_progression() {
    clear_buttons();
    current_menu_ = "progression";

    const ButtonStyle button_style = build_button_style(menu_data_.style_data);
    button_container_->add_theme_constant_override("separation", button_style.separation);

    auto *title = memnew(Label);
    title->set_text("YOUR UPGRADES");
    title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    title->add_theme_font_size_override("font_size", 36);
    title->add_theme_color_override("font_color", Color(1.0, 0.85, 0.3));
    button_container_->add_child(title);

    auto *progression = CampaignService::get_singleton();
    OwnedUpgradesPanel::Options owned_panel_options;
    owned_panel_options.min_size = get_progression_screen_size(this);
    owned_panel_options.layout = OwnedUpgradesPanel::Layout::VerticalGrid;
    owned_panel_options.grid_columns = 4;
    button_container_->add_child(OwnedUpgradesPanel::build(progression->build_owned_upgrade_cards(), owned_panel_options));

    auto *back_btn = memnew(Button);
    back_btn->set_text("Back");
    back_btn->set_custom_minimum_size(Vector2(160, button_style.minimum_size.y));
    back_btn->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
    back_btn->set_focus_mode(Control::FOCUS_NONE);
    apply_button_theme(back_btn, button_style, button_style.font_size);
    back_btn->connect("pressed", callable_mp(this, &MenuManager::on_button_pressed).bind(String("goto_menu"), String("game_menu")));
    button_container_->add_child(back_btn);
}

void MenuManager::build_options_ui(const MenuDefinition &menu_def) {
    const ButtonStyle button_style = build_button_style(menu_data_.style_data);
    const OptionsLayout options_layout = build_options_layout(menu_data_.style_data.get("options", Dictionary()));

    button_container_->add_theme_constant_override("separation", options_layout.row_separation);

    const auto current_mode = settings_state_.display_mode;
    const Vector2i current_size = settings_state_.resolution;
    const bool vsync_on = settings_state_.vsync_enabled;

    for (const auto &setting : menu_def.settings) {
        if (setting.kind == MenuSettingKind::SECTION) {
            add_section_label(button_container_, setting, options_layout);
            continue;
        }

        auto *row = create_option_row(setting, options_layout);
        const bool handled = try_add_display_mode_control(this, row, setting, button_style, options_layout, current_mode, display_mode_values_) ||
                             try_add_resolution_control(this, row, setting, button_style, options_layout, current_mode, current_size, resolution_dropdown_,
                                                        resolution_values_) ||
                             try_add_vsync_control(row, setting, options_layout, vsync_on, this) ||
                             try_add_volume_control(this, row, setting, options_layout, settings_state_, volume_labels_);

        if (handled) {
            button_container_->add_child(row);
        } else {
            row->queue_free();
            UtilityFunctions::printerr("MenuManager: Unknown option setting: ", setting.setting_id);
        }
    }

    add_back_button(this, button_container_, menu_def.back, button_style);
}

void MenuManager::on_display_mode_changed(int index) {
    if (index < 0 || std::cmp_greater_equal(index, display_mode_values_.size())) {
        return;
    }

    SettingsService::set_display_mode(settings_state_, display_mode_values_[index]);

    const bool windowed = settings_state_.display_mode == DisplayServer::WINDOW_MODE_WINDOWED;
    if (resolution_dropdown_) {
        apply_disabled_style(resolution_dropdown_, windowed);
    }

    SettingsService::save(settings_state_);
}

void MenuManager::on_resolution_changed(int index) {
    if (index < 0 || std::cmp_greater_equal(index, resolution_values_.size())) {
        return;
    }

    SettingsService::set_resolution(settings_state_, resolution_values_[index]);
    SettingsService::save(settings_state_);
}

void MenuManager::on_vsync_toggled(bool toggled) {
    SettingsService::set_vsync(settings_state_, toggled);
    SettingsService::save(settings_state_);
}

void MenuManager::on_volume_changed(double value, const String &bus_name) {
    SettingsService::set_bus_volume_percent(settings_state_, bus_name, value);

    for (auto &[name, label] : volume_labels_) {
        if (name == bus_name && label) {
            label->set_text(vformat("%d%%", static_cast<int>(value)));
            break;
        }
    }

    SettingsService::save(settings_state_);
}

} // namespace defn
