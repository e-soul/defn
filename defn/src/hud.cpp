#include "hud.h"
#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
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
    auto *loader = ResourceLoader::get_singleton();

    for (const auto &cfg : units) {
        // Find first frame of shoot animation
        String shoot_frame_path;
        for (const auto &[name, anim] : cfg.animations) {
            if (name == "shoot") {
                shoot_frame_path = vformat(anim.path_template, 0);
                break;
            }
        }

        auto *btn = memnew(Button);
        btn->set_custom_minimum_size(Vector2(130, 170));
        btn->set_focus_mode(Control::FOCUS_NONE);

        // Normal style
        Ref<StyleBoxFlat> style_normal;
        style_normal.instantiate();
        style_normal->set_bg_color(Color(0.12, 0.12, 0.18, 0.9));
        style_normal->set_border_width_all(2);
        style_normal->set_border_color(Color(0.4, 0.4, 0.5));
        style_normal->set_corner_radius_all(8);
        btn->add_theme_stylebox_override("normal", style_normal);

        // Hover style
        Ref<StyleBoxFlat> style_hover;
        style_hover.instantiate();
        style_hover->set_bg_color(Color(0.18, 0.18, 0.28, 0.95));
        style_hover->set_border_width_all(2);
        style_hover->set_border_color(Color(0.6, 0.6, 0.8));
        style_hover->set_corner_radius_all(8);
        btn->add_theme_stylebox_override("hover", style_hover);

        // Pressed style
        Ref<StyleBoxFlat> style_pressed;
        style_pressed.instantiate();
        style_pressed->set_bg_color(Color(0.08, 0.08, 0.14, 0.95));
        style_pressed->set_border_width_all(2);
        style_pressed->set_border_color(Color(0.5, 0.5, 0.7));
        style_pressed->set_corner_radius_all(8);
        btn->add_theme_stylebox_override("pressed", style_pressed);

        // Disabled style
        Ref<StyleBoxFlat> style_disabled;
        style_disabled.instantiate();
        style_disabled->set_bg_color(Color(0.08, 0.08, 0.1, 0.7));
        style_disabled->set_border_width_all(2);
        style_disabled->set_border_color(Color(0.25, 0.25, 0.3));
        style_disabled->set_corner_radius_all(8);
        btn->add_theme_stylebox_override("disabled", style_disabled);

        // Card content
        auto *vbox = memnew(VBoxContainer);
        vbox->set_anchors_preset(Control::PRESET_FULL_RECT);
        vbox->set_alignment(BoxContainer::ALIGNMENT_CENTER);
        vbox->add_theme_constant_override("separation", 4);
        vbox->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        btn->add_child(vbox);

        // Portrait (first frame of shoot animation)
        auto *portrait = memnew(TextureRect);
        portrait->set_custom_minimum_size(Vector2(110, 90));
        portrait->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
        portrait->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
        portrait->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        if (!shoot_frame_path.is_empty()) {
            Ref<Texture2D> tex = loader->load(shoot_frame_path);
            if (tex.is_valid()) {
                portrait->set_texture(tex);
            }
        }
        vbox->add_child(portrait);

        // Unit name
        auto *name_label = memnew(Label);
        name_label->set_text(cfg.name.capitalize());
        name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        name_label->add_theme_font_size_override("font_size", 14);
        name_label->add_theme_color_override("font_color", Color(0.9, 0.9, 0.95));
        name_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        vbox->add_child(name_label);

        // Cost
        auto *cost_label = memnew(Label);
        cost_label->set_text(vformat(String::utf8("\u26A1 %d"), cfg.cost));
        cost_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        cost_label->add_theme_font_size_override("font_size", 13);
        cost_label->add_theme_color_override("font_color", Color(0.3, 0.7, 1.0));
        cost_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        vbox->add_child(cost_label);

        btn->connect("pressed", callable_mp(this, &HUD::on_card_pressed).bind(cfg.name));
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
    bool victory = static_cast<bool>(stats.get("victory", false));
    int enemies_killed_val = static_cast<int>(stats.get("enemies_killed", 0));
    int kill_score_val = static_cast<int>(stats.get("kill_score", 0));
    int hearts_remaining = static_cast<int>(stats.get("hearts_remaining", 0));
    int hearts_total = static_cast<int>(stats.get("hearts_total", 3));
    int integrity_bonus = static_cast<int>(stats.get("integrity_bonus", 0));
    int completion_bonus = static_cast<int>(stats.get("completion_bonus", 0));
    int level_score = static_cast<int>(stats.get("level_score", 0));
    int new_total_score = static_cast<int>(stats.get("new_total_score", 0));
    String current_level_id = stats.get("current_level_id", "");
    String next_level_id = stats.get("next_level_id", "");
    Array new_unlocks = stats.get("new_unlocks", Array());

    // Dark overlay
    score_screen_overlay = memnew(ColorRect);
    score_screen_overlay->set_anchors_preset(Control::PRESET_FULL_RECT);
    score_screen_overlay->set_color(Color(0, 0, 0, 0.7));
    score_screen_overlay->set_mouse_filter(Control::MOUSE_FILTER_STOP);
    add_child(score_screen_overlay);

    // Center container
    auto *center = memnew(CenterContainer);
    center->set_anchors_preset(Control::PRESET_FULL_RECT);
    center->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    score_screen_overlay->add_child(center);

    // Panel
    score_screen_panel = memnew(PanelContainer);
    score_screen_panel->set_custom_minimum_size(Vector2(600, 0));
    Ref<StyleBoxFlat> panel_style;
    panel_style.instantiate();
    panel_style->set_bg_color(Color(0.08, 0.08, 0.14, 0.95));
    panel_style->set_border_width_all(2);
    panel_style->set_border_color(Color(0.4, 0.4, 0.6));
    panel_style->set_corner_radius_all(12);
    panel_style->set_content_margin_all(32);
    score_screen_panel->add_theme_stylebox_override("panel", panel_style);
    center->add_child(score_screen_panel);

    auto *vbox = memnew(VBoxContainer);
    vbox->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    vbox->add_theme_constant_override("separation", 8);
    score_screen_panel->add_child(vbox);

    // Title: VICTORY / DEFEAT
    auto *title = memnew(Label);
    title->set_text(victory ? "VICTORY" : "DEFEAT");
    title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    title->add_theme_font_size_override("font_size", 48);
    title->add_theme_color_override("font_color", victory ? Color(0.2, 1.0, 0.3) : Color(1.0, 0.2, 0.2));
    vbox->add_child(title);

    // Spacer
    auto *spacer1 = memnew(Control);
    spacer1->set_custom_minimum_size(Vector2(0, 12));
    vbox->add_child(spacer1);

    // Stats rows
    auto add_stat_row = [&](const String &label_text, const String &value_text) {
        auto *row = memnew(HBoxContainer);
        row->set_h_size_flags(Control::SIZE_EXPAND_FILL);

        auto *lbl = memnew(Label);
        lbl->set_text(label_text);
        lbl->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        lbl->add_theme_font_size_override("font_size", 22);
        lbl->add_theme_color_override("font_color", Color(0.8, 0.8, 0.85));
        row->add_child(lbl);

        auto *val = memnew(Label);
        val->set_text(value_text);
        val->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
        val->add_theme_font_size_override("font_size", 22);
        val->add_theme_color_override("font_color", Color(1.0, 1.0, 1.0));
        row->add_child(val);

        vbox->add_child(row);
    };

    add_stat_row("Enemies Killed:", vformat("%d", enemies_killed_val));
    add_stat_row("Kill Score:", vformat("%d", kill_score_val));
    add_stat_row("Hearts Remaining:", vformat("%d / %d", hearts_remaining, hearts_total));
    add_stat_row("Integrity Bonus:", vformat("%d", integrity_bonus));
    if (victory) {
        add_stat_row("Completion Bonus:", vformat("%d", completion_bonus));
    }

    // Separator
    auto *sep = memnew(ColorRect);
    sep->set_custom_minimum_size(Vector2(0, 2));
    sep->set_color(Color(0.4, 0.4, 0.5));
    vbox->add_child(sep);

    add_stat_row("Level Score:", vformat("%d", level_score));
    add_stat_row("Career Total:", vformat("%d", new_total_score));

    // New unlocks
    if (!new_unlocks.is_empty()) {
        auto *spacer2 = memnew(Control);
        spacer2->set_custom_minimum_size(Vector2(0, 8));
        vbox->add_child(spacer2);

        for (const auto &new_unlock : new_unlocks) {
            auto *unlock_label = memnew(Label);
            unlock_label->set_text(String(new_unlock));
            unlock_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
            unlock_label->add_theme_font_size_override("font_size", 20);
            unlock_label->add_theme_color_override("font_color", Color(1.0, 0.85, 0.3));
            vbox->add_child(unlock_label);
        }
    }

    // Spacer before buttons
    auto *spacer3 = memnew(Control);
    spacer3->set_custom_minimum_size(Vector2(0, 16));
    vbox->add_child(spacer3);

    // Action buttons
    auto *btn_row = memnew(HBoxContainer);
    btn_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    btn_row->add_theme_constant_override("separation", 16);
    vbox->add_child(btn_row);

    auto make_button = [](const String &text) -> Button * {
        auto *btn = memnew(Button);
        btn->set_text(text);
        btn->set_custom_minimum_size(Vector2(160, 50));
        btn->set_focus_mode(Control::FOCUS_NONE);
        btn->add_theme_font_size_override("font_size", 20);

        Ref<StyleBoxFlat> style;
        style.instantiate();
        style->set_bg_color(Color(0.15, 0.15, 0.25, 0.9));
        style->set_border_width_all(2);
        style->set_border_color(Color(0.4, 0.4, 0.6));
        style->set_corner_radius_all(8);
        btn->add_theme_stylebox_override("normal", style);

        Ref<StyleBoxFlat> hover;
        hover.instantiate();
        hover->set_bg_color(Color(0.2, 0.2, 0.35, 0.95));
        hover->set_border_width_all(2);
        hover->set_border_color(Color(0.6, 0.6, 0.8));
        hover->set_corner_radius_all(8);
        btn->add_theme_stylebox_override("hover", hover);

        return btn;
    };

    // Next Level button (only if victory and next level exists)
    if (victory && !next_level_id.is_empty()) {
        auto *next_btn = make_button("Next Level");
        next_btn->connect("pressed", callable_mp(this, &HUD::on_next_level_pressed).bind(next_level_id));
        btn_row->add_child(next_btn);
    }

    // Retry button
    auto *retry_btn = make_button("Retry");
    retry_btn->connect("pressed", callable_mp(this, &HUD::on_retry_pressed).bind(current_level_id));
    btn_row->add_child(retry_btn);

    // Main Menu button
    auto *menu_btn = make_button("Main Menu");
    menu_btn->connect("pressed", callable_mp(this, &HUD::on_main_menu_pressed));
    btn_row->add_child(menu_btn);
}

void HUD::on_next_level_pressed(const String &level_id) { emit_signal("score_screen_next_level", level_id); }

void HUD::on_retry_pressed(const String &level_id) { emit_signal("score_screen_retry", level_id); }

void HUD::on_main_menu_pressed() { emit_signal("score_screen_main_menu"); }

} // namespace defn
