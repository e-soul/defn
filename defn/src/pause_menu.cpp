#include "pause_menu.h"
#include "menu_data_loader.h"
#include "menu_style.h"
#include "scene_navigator.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

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
    const auto loaded_menu_data = MenuDataLoader::load("res://data/menu_data.json");
    if (!loaded_menu_data) {
        return false;
    }

    menu_data_ = *loaded_menu_data;
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

    const ButtonStyle button_style = build_button_style(style);

    button_container_->add_theme_constant_override("separation", button_style.separation);

    Array entries = pause_def.get("entries", Array());
    for (const auto &entry_variant : entries) {
        Dictionary entry = entry_variant;
        String label = entry.get("label", "???");
        String action = entry.get("action", "");

        auto *btn = memnew(Button);
        btn->set_text(label);
        btn->set_custom_minimum_size(button_style.minimum_size);
        btn->set_focus_mode(Control::FOCUS_NONE);
        apply_button_theme(btn, button_style, button_style.font_size);

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
    SceneNavigator::go_to_main_menu(get_tree());
}

} // namespace defn
