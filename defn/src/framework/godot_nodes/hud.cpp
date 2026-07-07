#include "hud.h"
#include "deploy_card_presenter.h"
#include "score_screen_presenter.h"
#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <utility>

namespace defn {

HUD::HUD() = default;

void HUD::_bind_methods() {
    ADD_SIGNAL(MethodInfo("deploy_requested", PropertyInfo(Variant::STRING, "unit_type")));
    ADD_SIGNAL(MethodInfo("score_screen_next_level", PropertyInfo(Variant::STRING, "level_id")));
    ADD_SIGNAL(MethodInfo("score_screen_retry", PropertyInfo(Variant::STRING, "level_id")));
    ADD_SIGNAL(MethodInfo("score_screen_main_menu"));
    ADD_SIGNAL(MethodInfo("score_screen_upgrade_selected", PropertyInfo(Variant::STRING, "upgrade_id")));
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
    ensure_heart_icons(3);

    level_label = memnew(Label);
    level_label->set_anchors_preset(Control::PRESET_TOP_WIDE);
    level_label->set_offset(Side::SIDE_LEFT, 16.0);
    level_label->set_offset(Side::SIDE_RIGHT, -16.0);
    level_label->set_offset(Side::SIDE_TOP, 52.0);
    level_label->set_offset(Side::SIDE_BOTTOM, 78.0);
    level_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    level_label->add_theme_font_size_override("font_size", 20);
    level_label->add_theme_color_override("font_color", Color(0.95, 0.95, 0.95));
    level_label->set_text("LEVEL");
    add_child(level_label);

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
        const String unit_type = String(cfg.name.c_str());
        auto *btn = DeployCardPresenter::create(cfg, callable_mp(this, &HUD::on_card_pressed).bind(unit_type));
        card_container->add_child(btn);
        deploy_cards.push_back({.unit_type = unit_type, .cost = cfg.cost, .button = btn});
    }
}

void HUD::set_level(int level_number, const String &level_name) {
    if (level_label == nullptr) {
        return;
    }

    String label_text;
    if (level_number > 0 && !level_name.is_empty()) {
        label_text = vformat("LEVEL %d - %s", level_number, level_name);
    } else if (level_number > 0) {
        label_text = vformat("LEVEL %d", level_number);
    } else {
        label_text = level_name;
    }

    level_label->set_text(label_text);
    level_label->set_visible(!label_text.is_empty());
}

void HUD::ensure_heart_icons(int count) {
    if (hearts_container == nullptr) {
        return;
    }

    while (std::cmp_less(heart_icons.size(), count)) {
        auto *heart = memnew(Label);
        heart->set_text(String::utf8("\u2665"));
        heart->add_theme_font_size_override("font_size", 32);
        heart->add_theme_color_override("font_color", Color(0.9, 0.15, 0.15));
        hearts_container->add_child(heart);
        heart_icons.push_back(heart);
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
    ensure_heart_icons(integrity);
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

void HUD::show_score_screen(const ScoreScreenModel &summary) {
    if (score_screen_overlay != nullptr && !score_screen_overlay->is_queued_for_deletion()) {
        score_screen_overlay->queue_free();
    }

    const ScoreScreenView view = ScoreScreenPresenter::show(this, summary,
                                                            {
                                                                .on_next_level = callable_mp(this, &HUD::on_next_level_pressed).bind(summary.next_level_id),
                                                                .on_retry = callable_mp(this, &HUD::on_retry_pressed).bind(summary.current_level_id),
                                                                .on_main_menu = callable_mp(this, &HUD::on_main_menu_pressed),
                                                                .on_select_upgrade = callable_mp(this, &HUD::on_upgrade_card_pressed),
                                                            });

    score_screen_overlay = view.overlay;
    score_screen_panel = view.panel;
}

void HUD::on_next_level_pressed(const String &level_id) { emit_signal("score_screen_next_level", level_id); }

void HUD::on_retry_pressed(const String &level_id) { emit_signal("score_screen_retry", level_id); }

void HUD::on_main_menu_pressed() { emit_signal("score_screen_main_menu"); }

void HUD::on_upgrade_card_pressed(const String &upgrade_id) { emit_signal("score_screen_upgrade_selected", upgrade_id); }

} // namespace defn
