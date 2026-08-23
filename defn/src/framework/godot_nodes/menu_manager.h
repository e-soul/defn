// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include "menu_flow_use_case.h"
#include "menu_models.h"
#include "menu_view_model.h"
#include "settings_models.h"
#include "ui_screen_scaffold.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <utility>
#include <vector>

namespace defn {

using namespace godot;

class SettingsRuntime;

class MenuManager : public Node2D {
    GDCLASS(MenuManager, Node2D)

  public:
    void _ready() override;
    void on_button_pressed(int intent_type, const String &target);
    void on_level_selected(const String &level_id);
    void on_display_mode_changed(int index);
    void on_resolution_changed(int index);
    void on_vsync_toggled(bool toggled);
    void on_volume_changed(double value, const String &bus_name);
    void connect_menu_sfx(BaseButton *button);
    [[nodiscard]] class UiSfxPlayer *sfx_player() const { return ui_sfx_player_; }

  protected:
    static void _bind_methods();

  private:
    bool load_menu_data();
    void setup_background();
    void build_career_score();
    void show_menu(const String &menu_name);
    void show_level_select();
    void show_progression();
    void handle_menu_intent(const MenuIntent &intent);
    void apply_menu_flow_result(const MenuFlowResult &result);
    /// Frees whatever screen is mounted and drops the per-screen control references with it.
    void clear_active_screen();
    void mount_screen(Control *screen);
    void build_options_ui(const MenuScreenViewModel &view_model, const UiScreenScaffold &scaffold);
    [[nodiscard]] static SettingsRuntime *settings_runtime_for_change();
    bool refresh_settings_snapshot();

    MenuContentData menu_data_;
    String current_menu_;
    SettingsState settings_state_;

    CanvasLayer *ui_layer_ = nullptr;
    TextureRect *background_ = nullptr;
    PanelContainer *career_score_plate_ = nullptr;
    Label *total_score_label_ = nullptr;
    Control *active_screen_ = nullptr;
    class UiSfxPlayer *ui_sfx_player_ = nullptr;

    // Options-menu state (reset by clear_active_screen)
    OptionButton *resolution_dropdown_ = nullptr;
    Button *vsync_toggle_ = nullptr;
    std::vector<std::pair<String, Label *>> volume_labels_;
    std::vector<DisplayServer::WindowMode> display_mode_values_;
    std::vector<Vector2i> resolution_values_;
};

} // namespace defn

#endif
