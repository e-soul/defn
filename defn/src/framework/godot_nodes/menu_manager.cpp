// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "menu_manager.h"
#include "campaign_map_view.h"
#include "data_paths.h"
#include "godot_string.h"
#include "menu_data_loader.h"
#include "progression_manager.h"
#include "progression_stats_screen_view.h"
#include "scene_navigator.h"
#include "settings_runtime.h"
#include "settings_use_case.h"
#include "ui_sfx_player.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"
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

MenuSettingViewKind to_setting_view_kind(MenuSettingKind kind) {
    switch (kind) {
    case MenuSettingKind::SECTION:
        return MenuSettingViewKind::Section;
    case MenuSettingKind::DISPLAY_MODE:
        return MenuSettingViewKind::DisplayMode;
    case MenuSettingKind::RESOLUTION:
        return MenuSettingViewKind::Resolution;
    case MenuSettingKind::VSYNC:
        return MenuSettingViewKind::Vsync;
    case MenuSettingKind::BUS_VOLUME:
        return MenuSettingViewKind::BusVolume;
    case MenuSettingKind::UNKNOWN:
        return MenuSettingViewKind::Unknown;
    }

    return MenuSettingViewKind::Unknown;
}

MenuScreenType to_screen_view_type(MenuDefinitionType type) { return type == MenuDefinitionType::OPTIONS ? MenuScreenType::Options : MenuScreenType::Buttons; }

MenuIntentType to_menu_intent_type(MenuActionType action_type) {
    switch (action_type) {
    case MenuActionType::GOTO_MENU:
        return MenuIntentType::GotoMenu;
    case MenuActionType::LEVEL_SELECT:
        return MenuIntentType::ShowLevelSelect;
    case MenuActionType::PROGRESSION:
        return MenuIntentType::ShowProgression;
    case MenuActionType::START_GAME:
        return MenuIntentType::StartGame;
    case MenuActionType::QUIT:
        return MenuIntentType::Quit;
    case MenuActionType::RESUME:
        return MenuIntentType::Resume;
    case MenuActionType::MAIN_MENU:
        return MenuIntentType::MainMenu;
    case MenuActionType::NONE:
        return MenuIntentType::None;
    }

    return MenuIntentType::None;
}

MenuActionPresentationInput to_action_input(const MenuAction &action) {
    return {
        .id = action.id,
        .label = action.label,
        .intent_type = to_menu_intent_type(action.action_type),
        .target = action.target,
    };
}

MenuSettingPresentationInput to_setting_input(const MenuSetting &setting) {
    MenuSettingPresentationInput input;
    input.id = setting.id;
    input.label = setting.label;
    input.setting_id = setting.setting_id;
    input.bus_name = setting.bus_name;
    input.kind = to_setting_view_kind(setting.kind);
    input.min_value = setting.min_value;
    input.max_value = setting.max_value;
    input.step_value = setting.step_value;
    input.options.reserve(setting.options.size());
    for (const auto &option : setting.options) {
        input.options.push_back({
            .label = option.label,
            .value = option.value,
        });
    }
    return input;
}

MenuScreenPresentationInput to_screen_input(const MenuDefinition &menu) {
    MenuScreenPresentationInput input;
    input.name = menu.name;
    input.type = to_screen_view_type(menu.type);
    input.entries.reserve(menu.entries.size());
    for (const auto &entry : menu.entries) {
        input.entries.push_back(to_action_input(entry));
    }
    input.settings.reserve(menu.settings.size());
    for (const auto &setting : menu.settings) {
        input.settings.push_back(to_setting_input(setting));
    }
    if (menu.back.has_value()) {
        input.back = to_action_input(*menu.back);
    }
    return input;
}

MenuIntent to_menu_intent(int intent_type, const String &target) {
    return {
        .type = static_cast<MenuIntentType>(intent_type),
        .target = to_std_string(target),
    };
}

godot::Vector2 get_progression_screen_size(Node *parent) {
    const godot::Vector2 fallback(800.0F, 360.0F);
    if (parent == nullptr || parent->get_viewport() == nullptr) {
        return fallback;
    }

    const godot::Vector2 viewport_size = parent->get_viewport()->get_visible_rect().size;
    if (viewport_size.x <= 0.0 || viewport_size.y <= 0.0) {
        return fallback;
    }

    return {viewport_size.x * PROGRESSION_SCREEN_WIDTH_RATIO, viewport_size.y * PROGRESSION_SCREEN_HEIGHT_RATIO};
}

void add_section_label(VBoxContainer *button_container, const MenuSettingViewModel &setting) {
    auto *section_label = make_label(to_godot_string(setting.label), "option_section");
    section_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    button_container->add_child(section_label);
}

HBoxContainer *create_option_row(const MenuSettingViewModel &setting) {
    auto *row = memnew(HBoxContainer);
    row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    row->add_theme_constant_override("separation", UiThemeProvider::spacing("section_gap"));

    auto *name_label = make_label(setting.label.empty() ? String("???") : to_godot_string(setting.label), "option_label");
    name_label->set_custom_minimum_size({UiThemeProvider::metric("option_label_width"), 0.0F});
    name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    row->add_child(name_label);

    return row;
}

godot::Vector2 option_control_size() {
    const UiButtonVariant *variant = UiThemeProvider::data().find_button("option_control");
    if (variant == nullptr) {
        return {300.0F, 40.0F};
    }
    return {static_cast<real_t>(variant->min_width), static_cast<real_t>(variant->min_height)};
}

bool try_add_display_mode_control(MenuManager *manager, HBoxContainer *row, const MenuSettingViewModel &setting, DisplayServer::WindowMode current_mode,
                                  std::vector<DisplayServer::WindowMode> &display_mode_values) {
    if (setting.kind != MenuSettingViewKind::DisplayMode) {
        return false;
    }

    auto *option_button = memnew(OptionButton);
    option_button->set_custom_minimum_size(option_control_size());
    option_button->set_focus_mode(Control::FOCUS_NONE);
    apply_button_style(option_button, "option_control");

    display_mode_values.clear();
    int selected_index = 0;
    int option_index = 0;
    for (const auto &option : setting.options) {
        option_button->add_item(to_godot_string(option.label));
        const auto mode_value = static_cast<DisplayServer::WindowMode>(std::stoi(option.value));
        display_mode_values.push_back(mode_value);
        if (static_cast<int>(mode_value) == static_cast<int>(current_mode)) {
            selected_index = option_index;
        }
        ++option_index;
    }

    option_button->select(selected_index);
    manager->connect_menu_sfx(option_button);
    option_button->connect("item_selected", callable_mp(manager, &MenuManager::on_display_mode_changed));
    row->add_child(option_button);
    return true;
}

bool try_add_resolution_control(MenuManager *manager, HBoxContainer *row, const MenuSettingViewModel &setting, DisplayServer::WindowMode current_mode,
                                const Vector2i &current_size, OptionButton *&resolution_dropdown, std::vector<Vector2i> &resolution_values) {
    if (setting.kind != MenuSettingViewKind::Resolution) {
        return false;
    }

    auto *option_button = memnew(OptionButton);
    option_button->set_custom_minimum_size(option_control_size());
    option_button->set_focus_mode(Control::FOCUS_NONE);
    apply_button_style(option_button, "option_control");

    resolution_values.clear();
    int selected_index = 0;
    int option_index = 0;
    for (const auto &option : setting.options) {
        option_button->add_item(to_godot_string(option.label));
        const SettingsResolution parsed = SettingsUseCase::parse_resolution_value(option.value);
        const Vector2i resolution(parsed.width, parsed.height);
        resolution_values.push_back(resolution);
        if (resolution == current_size) {
            selected_index = option_index;
        }
        ++option_index;
    }

    option_button->select(selected_index);

    const bool windowed = static_cast<int>(current_mode) == static_cast<int>(DisplayServer::WINDOW_MODE_WINDOWED);
    apply_enabled(option_button, windowed);

    resolution_dropdown = option_button;
    manager->connect_menu_sfx(option_button);
    option_button->connect("item_selected", callable_mp(manager, &MenuManager::on_resolution_changed));
    row->add_child(option_button);
    return true;
}

bool try_add_vsync_control(HBoxContainer *row, const MenuSettingViewModel &setting, bool vsync_on, MenuManager *manager) {
    if (setting.kind != MenuSettingViewKind::Vsync) {
        return false;
    }

    auto *check_button = memnew(CheckButton);
    check_button->set_custom_minimum_size(option_control_size());
    check_button->set_focus_mode(Control::FOCUS_NONE);
    check_button->set_pressed(vsync_on);
    manager->connect_menu_sfx(check_button);
    check_button->connect("toggled", callable_mp(manager, &MenuManager::on_vsync_toggled));
    row->add_child(check_button);
    return true;
}

bool try_add_volume_control(MenuManager *manager, HBoxContainer *row, const MenuSettingViewModel &setting, const SettingsState &settings_state,
                            std::vector<std::pair<String, Label *>> &volume_labels) {
    if (setting.kind != MenuSettingViewKind::BusVolume) {
        return false;
    }

    const String bus_name = setting.bus_name.empty() ? String("Master") : to_godot_string(setting.bus_name);
    const int min_value = setting.min_value;
    const int max_value = setting.max_value;
    const int step_value = setting.step_value;
    const double current_percent = SettingsUseCase::get_bus_volume_percent(settings_state, to_std_string(bus_name));

    auto *slider = memnew(HSlider);
    slider->set_custom_minimum_size(option_control_size());
    slider->set_min(min_value);
    slider->set_max(max_value);
    slider->set_step(step_value);
    slider->set_value(current_percent);
    slider->set_focus_mode(Control::FOCUS_NONE);
    slider->connect("value_changed", callable_mp(manager, &MenuManager::on_volume_changed).bind(bus_name));
    row->add_child(slider);

    auto *value_label = make_label(vformat("%d%%", static_cast<int>(current_percent)), "option_value");
    value_label->set_custom_minimum_size({UiThemeProvider::metric("option_value_width"), 0.0F});
    value_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    row->add_child(value_label);

    volume_labels.emplace_back(bus_name, value_label);
    return true;
}

void add_menu_button(MenuManager *manager, VBoxContainer *button_container, const MenuButtonViewModel &button_model) {
    const Callable pressed =
        callable_mp(manager, &MenuManager::on_button_pressed).bind(static_cast<int>(button_model.intent.type), to_godot_string(button_model.intent.target));
    auto *button = make_button(to_godot_string(button_model.label), "menu", pressed, manager->sfx_player());
    apply_enabled(button, button_model.enabled);
    button_container->add_child(button);
}

void add_back_button(MenuManager *manager, VBoxContainer *button_container, const std::optional<MenuButtonViewModel> &back) {
    if (!back.has_value()) {
        return;
    }

    add_menu_button(manager, button_container, *back);
}

} // namespace

void MenuManager::_bind_methods() {}

void MenuManager::_ready() {
    if (!load_menu_data()) {
        UtilityFunctions::printerr("MenuManager: Failed to load menu data");
        return;
    }

    refresh_settings_snapshot();

    UiThemeProvider::install(get_tree());

    ui_sfx_player_ = memnew(UiSfxPlayer);
    ui_sfx_player_->set_name("UiSfxPlayer");
    add_child(ui_sfx_player_);
    ui_sfx_player_->configure(UiThemeProvider::data().sfx);

    ui_layer_ = memnew(CanvasLayer);
    ui_layer_->set_name("UILayer");
    add_child(ui_layer_);

    setup_background();

    // Total score label (top right)
    auto *progression = CampaignService::get_singleton();
    total_score_label_ = make_label(vformat("Career Score: %d", progression->get_total_score()), "career_score");
    total_score_label_->set_anchors_preset(Control::PRESET_TOP_RIGHT);
    total_score_label_->set_offset(Side::SIDE_RIGHT, -UiThemeProvider::metric("career_score_right_margin", 24));
    total_score_label_->set_offset(Side::SIDE_TOP, UiThemeProvider::metric("career_score_top_margin", 16));
    total_score_label_->set_offset(Side::SIDE_LEFT, -UiThemeProvider::metric("career_score_width"));
    total_score_label_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    ui_layer_->add_child(total_score_label_);

    // Center container spanning the full viewport
    auto *center = memnew(CenterContainer);
    center->set_anchors_preset(Control::PRESET_FULL_RECT);
    center->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    ui_layer_->add_child(center);

    button_container_ = memnew(VBoxContainer);
    button_container_->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    center->add_child(button_container_);

    if (SceneNavigator::consume_campaign_map_request()) {
        show_level_select();
    } else {
        show_menu("main_menu");
    }
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
    const String bg_path = to_godot_string(menu_data_.background);
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
    background_->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
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

void MenuManager::clear_active_view() {
    if (active_fullscreen_view_ == nullptr) {
        return;
    }

    Control *view = active_fullscreen_view_;
    active_fullscreen_view_ = nullptr;
    view->queue_free();
}

void MenuManager::show_menu(const String &menu_name) {
    clear_active_view();
    clear_buttons();
    current_menu_ = menu_name;
    if (total_score_label_ != nullptr) {
        total_score_label_->set_visible(true);
    }

    const MenuDefinition *menu = menu_data_.find_menu(to_std_string(menu_name));
    if (menu == nullptr) {
        UtilityFunctions::printerr("MenuManager: Unknown menu: ", menu_name);
        return;
    }

    const MenuScreenViewModel view_model = build_menu_screen_view_model(to_screen_input(*menu));

    if (view_model.type == MenuScreenType::Options) {
        build_options_ui(view_model);
        return;
    }

    button_container_->add_theme_constant_override("separation",
                                                   UiThemeProvider::data().metric("menu_button_separation", UiThemeProvider::spacing("section_gap")));

    for (const auto &button_model : view_model.buttons) {
        add_menu_button(this, button_container_, button_model);
    }
}

void MenuManager::on_button_pressed(int intent_type, const String &target) { handle_menu_intent(to_menu_intent(intent_type, target)); }

void MenuManager::handle_menu_intent(const MenuIntent &intent) { apply_menu_flow_result(MenuFlowUseCase::handle(intent)); }

void MenuManager::apply_menu_flow_result(const MenuFlowResult &result) {
    switch (result.view) {
    case MenuFlowView::Menu:
        show_menu(to_godot_string(result.menu_name));
        break;
    case MenuFlowView::LevelSelect:
        show_level_select();
        break;
    case MenuFlowView::Progression:
        show_progression();
        break;
    case MenuFlowView::None:
        break;
    }

    if (result.navigation.has_value()) {
        SceneNavigator::navigate(get_tree(), *result.navigation);
    }
}

void MenuManager::show_level_select() {
    clear_active_view();
    clear_buttons();
    current_menu_ = "level_select";
    auto *progression = CampaignService::get_singleton();
    if (progression == nullptr) {
        UtilityFunctions::printerr("MenuManager: Campaign service unavailable");
        show_menu("game_menu");
        return;
    }

    auto *map_view = memnew(CampaignMapView);
    map_view->set_name("CampaignMapView");
    const Callable deploy_action = callable_mp(this, &MenuManager::on_level_selected);
    const Callable back_action = callable_mp(this, &MenuManager::on_button_pressed).bind(static_cast<int>(MenuIntentType::GotoMenu), String("game_menu"));
    ui_layer_->add_child(map_view);
    active_fullscreen_view_ = map_view;
    map_view->configure(progression, deploy_action, back_action, ui_sfx_player_);
    if (total_score_label_ != nullptr) {
        total_score_label_->set_visible(false);
    }
}

void MenuManager::on_level_selected(const String &level_id) {
    apply_menu_flow_result(MenuFlowUseCase(CampaignService::get_singleton()).select_level(to_std_string(level_id)));
}

void MenuManager::show_progression() {
    clear_active_view();
    clear_buttons();
    current_menu_ = "progression";
    if (total_score_label_ != nullptr) {
        total_score_label_->set_visible(true);
    }

    const ProgressionScreenViewModel view_model = build_progression_screen_view_model();
    auto *progression = CampaignService::get_singleton();
    auto *screen = memnew(ProgressionStatsScreenView);
    screen->set_custom_minimum_size(get_progression_screen_size(this));
    const Callable back_action = callable_mp(this, &MenuManager::on_button_pressed)
                                     .bind(static_cast<int>(view_model.back_button.intent.type), to_godot_string(view_model.back_button.intent.target));
    screen->configure(progression->build_progression_overview(), progression->build_owned_upgrade_cards_godot(), back_action, ui_sfx_player_);
    button_container_->add_child(screen);
}

void MenuManager::build_options_ui(const MenuScreenViewModel &view_model) {
    button_container_->add_theme_constant_override("separation", UiThemeProvider::spacing("md"));

    const auto current_mode = static_cast<DisplayServer::WindowMode>(settings_state_.display_mode);
    const Vector2i current_size(settings_state_.resolution.width, settings_state_.resolution.height);
    const bool vsync_on = settings_state_.vsync_enabled;

    for (const auto &setting : view_model.settings) {
        if (setting.kind == MenuSettingViewKind::Section) {
            add_section_label(button_container_, setting);
            continue;
        }

        auto *row = create_option_row(setting);
        const bool handled = try_add_display_mode_control(this, row, setting, current_mode, display_mode_values_) ||
                             try_add_resolution_control(this, row, setting, current_mode, current_size, resolution_dropdown_, resolution_values_) ||
                             try_add_vsync_control(row, setting, vsync_on, this) || try_add_volume_control(this, row, setting, settings_state_, volume_labels_);

        if (handled) {
            button_container_->add_child(row);
        } else {
            row->queue_free();
            UtilityFunctions::printerr("MenuManager: Unknown option setting: ", to_godot_string(setting.setting_id));
        }
    }

    add_back_button(this, button_container_, view_model.back_button);
}

SettingsRuntime *MenuManager::settings_runtime_for_change() {
    SettingsRuntime *runtime = SettingsRuntime::get_singleton();
    if (runtime == nullptr || !runtime->is_available()) {
        UtilityFunctions::printerr("MenuManager: Settings runtime is unavailable");
        return nullptr;
    }
    return runtime;
}

bool MenuManager::refresh_settings_snapshot() {
    SettingsRuntime *runtime = SettingsRuntime::get_singleton();
    if (runtime == nullptr || !runtime->is_available()) {
        settings_state_ = {};
        return false;
    }

    const SettingsState *state = runtime->get_state();
    if (state == nullptr) {
        settings_state_ = {};
        return false;
    }
    settings_state_ = *state;
    return true;
}

void MenuManager::on_display_mode_changed(int index) {
    if (index < 0 || std::cmp_greater_equal(index, display_mode_values_.size())) {
        return;
    }

    SettingsRuntime *runtime = settings_runtime_for_change();
    if (runtime == nullptr) {
        return;
    }

    const bool persisted = runtime->set_display_mode(static_cast<int>(display_mode_values_[index]));
    refresh_settings_snapshot();

    const bool windowed = settings_state_.display_mode == static_cast<int>(DisplayServer::WINDOW_MODE_WINDOWED);
    if (resolution_dropdown_) {
        apply_enabled(resolution_dropdown_, windowed);
    }

    if (!persisted) {
        UtilityFunctions::printerr("MenuManager: Failed to persist display mode");
    }
}

void MenuManager::on_resolution_changed(int index) {
    if (index < 0 || std::cmp_greater_equal(index, resolution_values_.size())) {
        return;
    }

    SettingsRuntime *runtime = settings_runtime_for_change();
    if (runtime == nullptr) {
        return;
    }

    const Vector2i resolution = resolution_values_[index];
    const bool persisted = runtime->set_resolution({.width = resolution.x, .height = resolution.y});
    refresh_settings_snapshot();
    if (!persisted) {
        UtilityFunctions::printerr("MenuManager: Failed to persist resolution");
    }
}

void MenuManager::on_vsync_toggled(bool toggled) {
    SettingsRuntime *runtime = settings_runtime_for_change();
    if (runtime == nullptr) {
        return;
    }

    const bool persisted = runtime->set_vsync(toggled);
    refresh_settings_snapshot();
    if (!persisted) {
        UtilityFunctions::printerr("MenuManager: Failed to persist VSync setting");
    }
}

void MenuManager::on_volume_changed(double value, const String &bus_name) {
    SettingsRuntime *runtime = settings_runtime_for_change();
    if (runtime == nullptr) {
        return;
    }

    const bool persisted = runtime->set_bus_volume_percent(bus_name, value);
    refresh_settings_snapshot();
    const double current_value = SettingsUseCase::get_bus_volume_percent(settings_state_, to_std_string(bus_name), value);

    for (auto &[name, label] : volume_labels_) {
        if (name == bus_name && label) {
            label->set_text(vformat("%d%%", static_cast<int>(current_value)));
            break;
        }
    }

    if (!persisted) {
        UtilityFunctions::printerr("MenuManager: Failed to persist audio setting for bus ", bus_name);
    }
}

void MenuManager::connect_menu_sfx(BaseButton *button) {
    if (ui_sfx_player_ != nullptr) {
        ui_sfx_player_->connect_menu_button(button);
    }
}

} // namespace defn
