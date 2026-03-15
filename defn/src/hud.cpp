#include "hud.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

HUD::HUD() = default;

void HUD::_bind_methods() {}

void HUD::_ready() {
    build_ui();
}

void HUD::build_ui() {
    // ==========================================================
    // Top bar
    // ==========================================================
    auto *top_bar = memnew(HBoxContainer);
    top_bar->set_anchors_preset(Control::PRESET_TOP_WIDE);
    top_bar->set_offset(Side::SIDE_LEFT, 16.0);
    top_bar->set_offset(Side::SIDE_RIGHT, -16.0);
    top_bar->set_offset(Side::SIDE_TOP, 8.0);
    top_bar->set_offset(Side::SIDE_BOTTOM, 56.0);
    top_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    add_child(top_bar);

    // Energy label (left)
    core_resource_label = memnew(Label);
    core_resource_label->set_text(String::utf8("\u26A1 Energy: 100"));
    core_resource_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    core_resource_label->add_theme_font_size_override("font_size", 28);
    core_resource_label->add_theme_color_override("font_color", Color(0.05, 0.2, 0.55));
    top_bar->add_child(core_resource_label);

    // Wave label (center)
    wave_label = memnew(Label);
    wave_label->set_text("WAVE 1 / 3");
    wave_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    wave_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    wave_label->add_theme_font_size_override("font_size", 28);
    wave_label->add_theme_color_override("font_color", Color(1, 1, 1));
    top_bar->add_child(wave_label);

    // Hearts container (right)
    hearts_container = memnew(HBoxContainer);
    hearts_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    hearts_container->set_alignment(BoxContainer::ALIGNMENT_END);
    top_bar->add_child(hearts_container);

    // Create 3 heart icons
    for (int i = 0; i < 3; ++i) {
        auto *heart = memnew(Label);
        heart->set_text(String::utf8("\u2665")); // ♥
        heart->add_theme_font_size_override("font_size", 32);
        heart->add_theme_color_override("font_color", Color(0.9, 0.15, 0.15));
        hearts_container->add_child(heart);
        heart_icons.push_back(heart);
    }

    // ==========================================================
    // Bottom bar
    // ==========================================================
    deploy_label = memnew(Label);
    deploy_label->set_text(String::utf8("\U0001F5E1 Click to Deploy Swordsman: 25 Energy"));
    deploy_label->set_anchors_preset(Control::PRESET_BOTTOM_WIDE);
    deploy_label->set_offset(Side::SIDE_TOP, -48.0);
    deploy_label->set_offset(Side::SIDE_BOTTOM, -8.0);
    deploy_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    deploy_label->add_theme_font_size_override("font_size", 24);
    deploy_label->add_theme_color_override("font_color", Color(0.85, 0.85, 0.85));
    add_child(deploy_label);

    // ==========================================================
    // End-game label (hidden by default)
    // ==========================================================
    end_game_label = memnew(Label);
    end_game_label->set_text("");
    end_game_label->set_anchors_preset(Control::PRESET_CENTER);
    end_game_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    end_game_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
    end_game_label->add_theme_font_size_override("font_size", 72);
    end_game_label->add_theme_color_override("font_color", Color(1, 1, 1));
    end_game_label->set_visible(false);
    add_child(end_game_label);
}

void HUD::update_core_resource(int value) {
    if (core_resource_label) {
        core_resource_label->set_text(vformat(String::utf8("\u26A1 Energy: %d"), value));
    }
}

void HUD::update_wave(int current, int total) {
    if (wave_label) {
        wave_label->set_text(vformat("WAVE %d / %d", current, total));
    }
}

void HUD::update_hearts(int integrity) {
    for (int i = 0; std::cmp_less(i, heart_icons.size()); ++i) {
        heart_icons[i]->set_visible(i < integrity);
    }
}

void HUD::update_deploy_button(bool can_afford) {
    if (deploy_label) {
        Color col = can_afford ? Color(0.85, 0.85, 0.85) : Color(0.4, 0.4, 0.4);
        deploy_label->add_theme_color_override("font_color", col);
    }
}

void HUD::show_victory() {
    if (end_game_label) {
        end_game_label->set_text("VICTORY");
        end_game_label->add_theme_color_override("font_color", Color(0.2, 1.0, 0.3));
        end_game_label->set_visible(true);
    }
}

void HUD::show_defeat() {
    if (end_game_label) {
        end_game_label->set_text("DEFEAT");
        end_game_label->add_theme_color_override("font_color", Color(1.0, 0.2, 0.2));
        end_game_label->set_visible(true);
    }
}

} // namespace defn
