#include "hud.h"
#include "deploy_card_presenter.h"
#include "score_screen_presenter.h"
#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

HUD::HUD() = default;

void HUD::_bind_methods() {
    ADD_SIGNAL(MethodInfo("deploy_requested", PropertyInfo(Variant::STRING, "unit_type")));
    ADD_SIGNAL(MethodInfo("score_screen_next_level", PropertyInfo(Variant::STRING, "level_id")));
    ADD_SIGNAL(MethodInfo("score_screen_retry", PropertyInfo(Variant::STRING, "level_id")));
    ADD_SIGNAL(MethodInfo("score_screen_main_menu"));
}

void HUD::_ready() { build_ui(); }

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

    // Score label
    score_label = memnew(Label);
    score_label->set_text("Score: 0");
    score_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    score_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    score_label->add_theme_font_size_override("font_size", 24);
    score_label->add_theme_color_override("font_color", Color(1.0, 0.85, 0.3));
    top_bar->add_child(score_label);

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
    // Deploy card container (bottom center)
    // ==========================================================
    card_container = memnew(HBoxContainer);
    card_container->set_anchor(SIDE_LEFT, 0.5);
    card_container->set_anchor(SIDE_RIGHT, 0.5);
    card_container->set_anchor(SIDE_TOP, 1.0);
    card_container->set_anchor(SIDE_BOTTOM, 1.0);
    card_container->set_h_grow_direction(Control::GROW_DIRECTION_BOTH);
    card_container->set_v_grow_direction(Control::GROW_DIRECTION_BEGIN);
    card_container->set_offset(Side::SIDE_BOTTOM, -10.0);
    card_container->add_theme_constant_override("separation", 12);
    add_child(card_container);
}

void HUD::set_friendly_units(const std::vector<UnitConfig> &units) {
    for (const auto &cfg : units) {
        auto *btn = DeployCardPresenter::create(cfg, callable_mp(this, &HUD::on_card_pressed).bind(cfg.name));
        card_container->add_child(btn);
        deploy_cards.push_back({.unit_type = cfg.name, .cost = cfg.cost, .button = btn});
    }
}

void HUD::on_card_pressed(const String &unit_type) { emit_signal("deploy_requested", unit_type); }

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

void HUD::update_card_affordability(int energy) {
    for (auto &card : deploy_cards) {
        bool can_afford = energy >= card.cost;
        if (card.button) {
            card.button->set_disabled(!can_afford);
            card.button->set_modulate(can_afford ? Color(1, 1, 1, 1) : Color(0.5, 0.5, 0.5, 0.7));
        }
    }
}

void HUD::update_score(int score) {
    if (score_label) {
        score_label->set_text(vformat("Score: %d", score));
    }
}

void HUD::show_score_screen(const Dictionary &stats) {
    if (score_screen_overlay != nullptr && !score_screen_overlay->is_queued_for_deletion()) {
        score_screen_overlay->queue_free();
    }

    const String current_level_id = stats.get("current_level_id", "");
    const String next_level_id = stats.get("next_level_id", "");
    const ScoreScreenView view = ScoreScreenPresenter::show(this, stats,
                                                            {
                                                                .on_next_level = callable_mp(this, &HUD::on_next_level_pressed).bind(next_level_id),
                                                                .on_retry = callable_mp(this, &HUD::on_retry_pressed).bind(current_level_id),
                                                                .on_main_menu = callable_mp(this, &HUD::on_main_menu_pressed),
                                                            });

    score_screen_overlay = view.overlay;
    score_screen_panel = view.panel;
}

void HUD::on_next_level_pressed(const String &level_id) { emit_signal("score_screen_next_level", level_id); }

void HUD::on_retry_pressed(const String &level_id) { emit_signal("score_screen_retry", level_id); }

void HUD::on_main_menu_pressed() { emit_signal("score_screen_main_menu"); }

} // namespace defn
