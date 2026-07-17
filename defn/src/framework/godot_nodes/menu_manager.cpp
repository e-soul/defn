#include "menu_manager.h"
#include "data_paths.h"
#include "godot_string.h"
#include "menu_data_loader.h"
#include "menu_style.h"
#include "progression_manager.h"
#include "progression_presentation.h"
#include "progression_stats_screen_view.h"
#include "scene_navigator.h"
#include "settings_service.h"
#include "ui_sfx_player.h"
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
    if (parent == nullptr || parent->get_viewport() == nullptr) {
        return make_size(800, 360);
    }

    const godot::Vector2 viewport_size = parent->get_viewport()->get_visible_rect().size;
    if (viewport_size.x <= 0.0 || viewport_size.y <= 0.0) {
        return make_size(800, 360);
    }

    return {viewport_size.x * PROGRESSION_SCREEN_WIDTH_RATIO, viewport_size.y * PROGRESSION_SCREEN_HEIGHT_RATIO};
}

void add_section_label(VBoxContainer *button_container, const MenuSettingViewModel &setting, const OptionsLayout &options_layout) {
    auto *section_label = memnew(Label);
    section_label->set_text(to_godot_string(setting.label));
    section_label->add_theme_font_size_override("font_size", options_layout.section_font_size);
    section_label->add_theme_color_override("font_color", options_layout.section_color);
    section_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    button_container->add_child(section_label);
}

HBoxContainer *create_option_row(const MenuSettingViewModel &setting, const OptionsLayout &options_layout) {
    auto *row = memnew(HBoxContainer);
    row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    row->add_theme_constant_override("separation", 16);

    auto *name_label = memnew(Label);
    name_label->set_text(setting.label.empty() ? String("???") : to_godot_string(setting.label));
    name_label->set_custom_minimum_size(options_layout.label_minimum_size);
    name_label->add_theme_font_size_override("font_size", options_layout.label_font_size);
    name_label->add_theme_color_override("font_color", options_layout.label_color);
    name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    row->add_child(name_label);

    return row;
}

bool try_add_display_mode_control(MenuManager *manager, HBoxContainer *row, const MenuSettingViewModel &setting, const ButtonStyle &button_style,
                                  const OptionsLayout &options_layout, DisplayServer::WindowMode current_mode,
                                  std::vector<DisplayServer::WindowMode> &display_mode_values) {
    if (setting.kind != MenuSettingViewKind::DisplayMode) {
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

bool try_add_resolution_control(MenuManager *manager, HBoxContainer *row, const MenuSettingViewModel &setting, const ButtonStyle &button_style,
                                const OptionsLayout &options_layout, DisplayServer::WindowMode current_mode, const Vector2i &current_size,
                                OptionButton *&resolution_dropdown, std::vector<Vector2i> &resolution_values) {
    if (setting.kind != MenuSettingViewKind::Resolution) {
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
        option_button->add_item(to_godot_string(option.label));
        const Vector2i resolution = SettingsService::parse_resolution_value(to_godot_string(option.value));
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
    manager->connect_menu_sfx(option_button);
    option_button->connect("item_selected", callable_mp(manager, &MenuManager::on_resolution_changed));
    row->add_child(option_button);
    return true;
}

bool try_add_vsync_control(HBoxContainer *row, const MenuSettingViewModel &setting, const OptionsLayout &options_layout, bool vsync_on, MenuManager *manager) {
    if (setting.kind != MenuSettingViewKind::Vsync) {
        return false;
    }

    auto *check_button = memnew(CheckButton);
    check_button->set_custom_minimum_size(options_layout.control_minimum_size);
    check_button->set_focus_mode(Control::FOCUS_NONE);
    check_button->set_pressed(vsync_on);
    manager->connect_menu_sfx(check_button);
    check_button->connect("toggled", callable_mp(manager, &MenuManager::on_vsync_toggled));
    row->add_child(check_button);
    return true;
}

bool try_add_volume_control(MenuManager *manager, HBoxContainer *row, const MenuSettingViewModel &setting, const OptionsLayout &options_layout,
                            const SettingsState &settings_state, std::vector<std::pair<String, Label *>> &volume_labels) {
    if (setting.kind != MenuSettingViewKind::BusVolume) {
        return false;
    }

    const String bus_name = setting.bus_name.empty() ? String("Master") : to_godot_string(setting.bus_name);
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

void add_menu_button(MenuManager *manager, VBoxContainer *button_container, const MenuButtonViewModel &button_model, const ButtonStyle &button_style,
                     real_t width_override = 0.0F) {
    auto *button = memnew(Button);
    button->set_text(to_godot_string(button_model.label));
    const godot::Vector2 minimum_size = width_override > 0.0F ? godot::Vector2(width_override, button_style.minimum_size.y) : button_style.minimum_size;
    button->set_custom_minimum_size(minimum_size);
    button->set_focus_mode(Control::FOCUS_NONE);
    apply_button_theme(button, button_style, button_style.font_size);

    if (!button_model.enabled) {
        button->set_disabled(true);
        button->set_modulate(godot::Color(0.5, 0.5, 0.5, 0.7));
    }

    manager->connect_menu_sfx(button);
    button->connect(
        "pressed",
        callable_mp(manager, &MenuManager::on_button_pressed).bind(static_cast<int>(button_model.intent.type), to_godot_string(button_model.intent.target)));
    button_container->add_child(button);
}

void add_back_button(MenuManager *manager, VBoxContainer *button_container, const std::optional<MenuButtonViewModel> &back, const ButtonStyle &button_style) {
    if (!back.has_value()) {
        return;
    }

    add_menu_button(manager, button_container, *back, button_style);
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

    ui_sfx_player_ = memnew(UiSfxPlayer);
    ui_sfx_player_->set_name("UiSfxPlayer");
    add_child(ui_sfx_player_);
    ui_sfx_player_->configure(menu_data_.sfx);

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
    total_score_label_->add_theme_color_override("font_color", godot::Color(1.0, 0.85, 0.3));
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

void MenuManager::show_menu(const String &menu_name) {
    clear_buttons();
    current_menu_ = menu_name;

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

    const ButtonStyle button_style = build_button_style(menu_data_.style);

    button_container_->add_theme_constant_override("separation", button_style.separation);

    for (const auto &button_model : view_model.buttons) {
        add_menu_button(this, button_container_, button_model, button_style);
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
    clear_buttons();
    current_menu_ = "level_select";

    const ButtonStyle button_style = build_button_style(menu_data_.style);

    button_container_->add_theme_constant_override("separation", button_style.separation);

    auto *title = memnew(Label);
    std::vector<LevelSelectRowViewModel> levels;
    auto *progression = CampaignService::get_singleton();
    const auto level_data = progression->get_level_unlock_data();
    levels.reserve(level_data.size());
    for (const auto &level : level_data) {
        levels.push_back({
            .level_id = level.level_id,
            .label = ProgressionPresentation::format_level_button_label(level, progression->is_level_unlocked(level.level_id),
                                                                        progression->is_level_completed(level.level_id),
                                                                        progression->get_highest_level_score(level.level_id)),
            .unlocked = progression->is_level_unlocked(level.level_id),
        });
    }
    const LevelSelectViewModel view_model = build_level_select_view_model(std::move(levels));

    title->set_text(to_godot_string(view_model.title));
    title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    title->add_theme_font_size_override("font_size", 36);
    title->add_theme_color_override("font_color", godot::Color(1.0, 0.85, 0.3));
    button_container_->add_child(title);

    for (const auto &level : view_model.levels) {
        auto *btn = memnew(Button);
        btn->set_text(to_godot_string(level.label));
        btn->set_custom_minimum_size(button_style.minimum_size);
        btn->set_focus_mode(Control::FOCUS_NONE);
        apply_button_theme(btn, button_style, button_style.font_size);
        connect_menu_sfx(btn);

        if (!level.unlocked) {
            btn->set_disabled(true);
            btn->set_modulate(godot::Color(0.5, 0.5, 0.5, 0.7));
        } else {
            btn->connect("pressed", callable_mp(this, &MenuManager::on_level_selected).bind(to_godot_string(level.level_id)));
        }

        button_container_->add_child(btn);
    }

    add_menu_button(this, button_container_, view_model.back_button, button_style);
}

void MenuManager::on_level_selected(const String &level_id) {
    apply_menu_flow_result(MenuFlowUseCase(CampaignService::get_singleton()).select_level(to_std_string(level_id)));
}

void MenuManager::show_progression() {
    clear_buttons();
    current_menu_ = "progression";

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
    const ButtonStyle button_style = build_button_style(menu_data_.style);
    const OptionsLayout options_layout = build_options_layout(menu_data_.style.options);

    button_container_->add_theme_constant_override("separation", options_layout.row_separation);

    const auto current_mode = static_cast<DisplayServer::WindowMode>(settings_state_.display_mode);
    const Vector2i current_size(settings_state_.resolution.width, settings_state_.resolution.height);
    const bool vsync_on = settings_state_.vsync_enabled;

    for (const auto &setting : view_model.settings) {
        if (setting.kind == MenuSettingViewKind::Section) {
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
            UtilityFunctions::printerr("MenuManager: Unknown option setting: ", to_godot_string(setting.setting_id));
        }
    }

    add_back_button(this, button_container_, view_model.back_button, button_style);
}

void MenuManager::on_display_mode_changed(int index) {
    if (index < 0 || std::cmp_greater_equal(index, display_mode_values_.size())) {
        return;
    }

    SettingsService::set_display_mode(settings_state_, display_mode_values_[index]);

    const bool windowed = settings_state_.display_mode == static_cast<int>(DisplayServer::WINDOW_MODE_WINDOWED);
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

void MenuManager::connect_menu_sfx(BaseButton *button) {
    if (ui_sfx_player_ != nullptr) {
        ui_sfx_player_->connect_menu_button(button);
    }
}

} // namespace defn
