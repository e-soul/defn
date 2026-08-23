// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "pause_menu.h"
#include "data_paths.h"
#include "godot_string.h"
#include "menu_data_loader.h"
#include "ui_screen_scaffold.h"
#include "ui_sfx_player.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

void PauseMenu::_bind_methods() { ADD_SIGNAL(MethodInfo("main_menu_requested")); }

void PauseMenu::_ready() {
    UiThemeProvider::install(get_tree());
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
    const auto loaded_menu_data = MenuDataLoader::load(DataPaths::MENU_DATA);
    if (!loaded_menu_data) {
        return false;
    }

    menu_data_ = *loaded_menu_data;
    return true;
}

void PauseMenu::build_ui() {
    const MenuDefinition *pause_menu = menu_data_.find_menu("pause_menu");
    if (pause_menu == nullptr) {
        UtilityFunctions::printerr("PauseMenu: Missing pause_menu definition");
        return;
    }

    ui_sfx_player_ = memnew(UiSfxPlayer);
    ui_sfx_player_->set_name("UiSfxPlayer");
    add_child(ui_sfx_player_);
    ui_sfx_player_->configure(UiThemeProvider::data().sfx);

    // The scrim, the panel and the heading all come from the shared chrome, so pausing looks like every other
    // screen the game puts in front of the player rather than a bare stack of buttons.
    const UiScreenScaffold scaffold = build_screen(this, {
                                                             .title = to_godot_string(pause_menu->title),
                                                             .show_backdrop = true,
                                                             .scrollable_body = false,
                                                             .fit_content = true,
                                                         });
    if (scaffold.root == nullptr) {
        return;
    }
    screen_ = scaffold.root;
    button_container_ = scaffold.body;

    button_container_->add_theme_constant_override("separation",
                                                   UiThemeProvider::data().metric("menu_button_separation", UiThemeProvider::spacing("section_gap")));

    for (const auto &entry : pause_menu->entries) {
        Callable pressed;
        if (entry.action_type == MenuActionType::RESUME) {
            pressed = callable_mp(this, &PauseMenu::on_resume);
        } else if (entry.action_type == MenuActionType::MAIN_MENU) {
            pressed = callable_mp(this, &PauseMenu::on_main_menu);
        }

        auto *btn = make_button(entry.label.empty() ? String("???") : to_godot_string(entry.label), "menu", pressed, ui_sfx_player_);
        button_container_->add_child(btn);
    }
}

void PauseMenu::toggle_pause() { set_paused(!paused_); }

void PauseMenu::set_paused(bool paused) {
    paused_ = paused;
    get_tree()->set_pause(paused_);

    if (screen_ != nullptr) {
        screen_->set_visible(paused_);
        screen_->set_process_mode(paused_ ? PROCESS_MODE_ALWAYS : PROCESS_MODE_DISABLED);
    }
}

void PauseMenu::on_resume() { set_paused(false); }

void PauseMenu::on_main_menu() {
    set_paused(false);
    emit_signal("main_menu_requested");
}

} // namespace defn
