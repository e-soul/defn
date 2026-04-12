#include "score_screen_presenter.h"

#include "progression_presentation.h"
#include "upgrade_card_presenter.h"
#include "variant_tools.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

namespace defn {

namespace {

Button *make_action_button(const String &text) {
    auto *button = memnew(Button);
    button->set_text(text);
    button->set_custom_minimum_size(Vector2(160, 50));
    button->set_focus_mode(Control::FOCUS_NONE);
    button->add_theme_font_size_override("font_size", 20);

    Ref<StyleBoxFlat> normal_style;
    normal_style.instantiate();
    normal_style->set_bg_color(Color(0.15, 0.15, 0.25, 0.9));
    normal_style->set_border_width_all(2);
    normal_style->set_border_color(Color(0.4, 0.4, 0.6));
    normal_style->set_corner_radius_all(8);
    button->add_theme_stylebox_override("normal", normal_style);

    Ref<StyleBoxFlat> hover_style;
    hover_style.instantiate();
    hover_style->set_bg_color(Color(0.2, 0.2, 0.35, 0.95));
    hover_style->set_border_width_all(2);
    hover_style->set_border_color(Color(0.6, 0.6, 0.8));
    hover_style->set_corner_radius_all(8);
    button->add_theme_stylebox_override("hover", hover_style);

    return button;
}

void add_stat_row(VBoxContainer *parent, const String &label_text, const String &value_text) {
    auto *row = memnew(HBoxContainer);
    row->set_h_size_flags(Control::SIZE_EXPAND_FILL);

    auto *label = memnew(Label);
    label->set_text(label_text);
    label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    label->add_theme_font_size_override("font_size", 22);
    label->add_theme_color_override("font_color", Color(0.8, 0.8, 0.85));
    row->add_child(label);

    auto *value = memnew(Label);
    value->set_text(value_text);
    value->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    value->add_theme_font_size_override("font_size", 22);
    value->add_theme_color_override("font_color", Color(1.0, 1.0, 1.0));
    row->add_child(value);

    parent->add_child(row);
}

void add_spacer(VBoxContainer *parent, real_t height) {
    auto *spacer = memnew(Control);
    spacer->set_custom_minimum_size(Vector2(0, height));
    parent->add_child(spacer);
}

void connect_if_valid(Button *button, const Callable &callable) {
    if (button != nullptr && callable.is_valid()) {
        button->connect("pressed", callable);
    }
}

void apply_button_enabled(Button *button, bool enabled) {
    if (button == nullptr) {
        return;
    }

    button->set_disabled(!enabled);
    button->set_modulate(enabled ? Color(1, 1, 1, 1) : Color(0.6, 0.6, 0.65, 0.85));
}

void add_upgrade_section(VBoxContainer *content, const String &reward_title, const String &reward_subtitle, const Array &available_upgrades,
                         const Dictionary &selected_upgrade, const Callable &on_select_upgrade) {
    if (content == nullptr || available_upgrades.is_empty()) {
        return;
    }

    const String selected_upgrade_id = String(selected_upgrade.get("id", ""));

    add_spacer(content, 12);

    auto *section_title = memnew(Label);
    section_title->set_text(reward_title.is_empty() ? String("CHOOSE 1 UPGRADE") : reward_title);
    section_title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    section_title->add_theme_font_size_override("font_size", 24);
    section_title->add_theme_color_override("font_color", Color(1.0, 0.85, 0.3));
    content->add_child(section_title);

    if (!reward_subtitle.is_empty()) {
        auto *subtitle = memnew(Label);
        subtitle->set_text(reward_subtitle);
        subtitle->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        subtitle->add_theme_font_size_override("font_size", 18);
        subtitle->add_theme_color_override("font_color", Color(0.84, 0.88, 0.95));
        content->add_child(subtitle);
    }

    if (!selected_upgrade_id.is_empty()) {
        auto *summary = memnew(Label);
        summary->set_text(vformat("Selected: %s %s", String(selected_upgrade.get("emoji", "")), String(selected_upgrade.get("name", "Upgrade"))));
        summary->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        summary->add_theme_font_size_override("font_size", 18);
        summary->add_theme_color_override("font_color", Color(0.92, 0.95, 1.0));
        content->add_child(summary);
    }

    add_spacer(content, 8);

    auto *card_row = memnew(HBoxContainer);
    card_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    card_row->add_theme_constant_override("separation", 12);
    content->add_child(card_row);

    for (const Variant &card_variant : available_upgrades) {
        const Dictionary card = card_variant;
        const String card_id = String(card.get("id", ""));
        const bool selected = !selected_upgrade_id.is_empty() && card_id == selected_upgrade_id;

        Callable pressed_action;
        if (on_select_upgrade.is_valid() && !card_id.is_empty()) {
            pressed_action = on_select_upgrade.bind(card_id);
        }

        auto *card_button = UpgradeCardPresenter::create(card, selected, false, pressed_action);
        card_row->add_child(card_button);
    }
}

} // namespace

ScoreScreenView ScoreScreenPresenter::show(Node *parent, const Dictionary &stats, const ScoreScreenActions &actions) {
    if (parent == nullptr) {
        return {};
    }

    const bool victory = VariantTools::as_bool(stats.get("victory", false));
    const int enemies_killed = VariantTools::as_int(stats.get("enemies_killed", 0));
    const int kill_score = VariantTools::as_int(stats.get("kill_score", 0));
    const int hearts_remaining = VariantTools::as_int(stats.get("hearts_remaining", 0));
    const int hearts_total = VariantTools::as_int(stats.get("hearts_total", 3));
    const int integrity_bonus = VariantTools::as_int(stats.get("integrity_bonus", 0));
    const int completion_bonus = VariantTools::as_int(stats.get("completion_bonus", 0));
    const int level_score = VariantTools::as_int(stats.get("level_score", 0));
    const int new_total_score = VariantTools::as_int(stats.get("new_total_score", 0));
    const String next_level_id = stats.get("next_level_id", "");
    const Array new_unlocks = stats.get("new_unlocks", Array());
    const Array available_upgrades = stats.get("available_upgrades", Array());
    const Dictionary selected_upgrade = stats.get("selected_upgrade", Dictionary());
    const String reward_title = String(stats.get("reward_title", ""));
    const String reward_subtitle = String(stats.get("reward_subtitle", ""));
    const String frontier_level_id = String(stats.get("frontier_level_id", ""));
    const int rescue_points_gained = VariantTools::as_int(stats.get("rescue_points_gained", 0));
    const int rescue_points_bank = VariantTools::as_int(stats.get("rescue_points_bank", 0));
    const int next_rescue_cost = VariantTools::as_int(stats.get("next_rescue_cost", 0));
    const String selected_upgrade_id = String(selected_upgrade.get("id", ""));
    const bool reward_choice_required = !available_upgrades.is_empty() && selected_upgrade_id.is_empty();

    ScoreScreenView view;

    view.overlay = memnew(ColorRect);
    view.overlay->set_anchors_preset(Control::PRESET_FULL_RECT);
    view.overlay->set_color(Color(0, 0, 0, 0.7));
    view.overlay->set_mouse_filter(Control::MOUSE_FILTER_STOP);
    parent->add_child(view.overlay);

    auto *center = memnew(CenterContainer);
    center->set_anchors_preset(Control::PRESET_FULL_RECT);
    center->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    view.overlay->add_child(center);

    view.panel = memnew(PanelContainer);
    view.panel->set_custom_minimum_size(Vector2(600, 0));
    Ref<StyleBoxFlat> panel_style;
    panel_style.instantiate();
    panel_style->set_bg_color(Color(0.08, 0.08, 0.14, 0.95));
    panel_style->set_border_width_all(2);
    panel_style->set_border_color(Color(0.4, 0.4, 0.6));
    panel_style->set_corner_radius_all(12);
    panel_style->set_content_margin_all(32);
    view.panel->add_theme_stylebox_override("panel", panel_style);
    center->add_child(view.panel);

    auto *content = memnew(VBoxContainer);
    content->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    content->add_theme_constant_override("separation", 8);
    view.panel->add_child(content);

    auto *title = memnew(Label);
    title->set_text(victory ? "VICTORY" : "DEFEAT");
    title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    title->add_theme_font_size_override("font_size", 48);
    title->add_theme_color_override("font_color", victory ? Color(0.2, 1.0, 0.3) : Color(1.0, 0.2, 0.2));
    content->add_child(title);

    add_spacer(content, 12);
    add_stat_row(content, "Enemies Killed:", vformat("%d", enemies_killed));
    add_stat_row(content, "Kill Score:", vformat("%d", kill_score));
    add_stat_row(content, "Hearts Remaining:", vformat("%d / %d", hearts_remaining, hearts_total));
    add_stat_row(content, "Integrity Bonus:", vformat("%d", integrity_bonus));
    if (victory) {
        add_stat_row(content, "Completion Bonus:", vformat("%d", completion_bonus));
    }

    auto *separator = memnew(ColorRect);
    separator->set_custom_minimum_size(Vector2(0, 2));
    separator->set_color(Color(0.4, 0.4, 0.5));
    content->add_child(separator);

    add_stat_row(content, "Level Score:", vformat("%d", level_score));
    add_stat_row(content, "Career Total:", vformat("%d", new_total_score));
    if (rescue_points_gained > 0) {
        add_stat_row(content, "Rescue Points:", vformat("+%d", rescue_points_gained));
    }
    if (!frontier_level_id.is_empty() && next_rescue_cost > 0) {
        add_stat_row(content, "Frontier Rescue:",
                     vformat("%s %d / %d", ProgressionPresentation::format_level_name(frontier_level_id), rescue_points_bank, next_rescue_cost));
    }

    if (!new_unlocks.is_empty()) {
        add_spacer(content, 8);
        for (const auto &new_unlock : new_unlocks) {
            auto *unlock_label = memnew(Label);
            unlock_label->set_text(String(new_unlock));
            unlock_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
            unlock_label->add_theme_font_size_override("font_size", 20);
            unlock_label->add_theme_color_override("font_color", Color(1.0, 0.85, 0.3));
            content->add_child(unlock_label);
        }
    }

    add_upgrade_section(content, reward_title, reward_subtitle, available_upgrades, selected_upgrade, actions.on_select_upgrade);

    add_spacer(content, 16);

    auto *button_row = memnew(HBoxContainer);
    button_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    button_row->add_theme_constant_override("separation", 16);
    content->add_child(button_row);

    if (victory && !next_level_id.is_empty()) {
        auto *next_button = make_action_button("Next Level");
        connect_if_valid(next_button, actions.on_next_level);
        apply_button_enabled(next_button, !reward_choice_required);
        button_row->add_child(next_button);
    }

    auto *retry_button = make_action_button("Retry");
    connect_if_valid(retry_button, actions.on_retry);
    apply_button_enabled(retry_button, !reward_choice_required);
    button_row->add_child(retry_button);

    auto *menu_button = make_action_button("Main Menu");
    connect_if_valid(menu_button, actions.on_main_menu);
    apply_button_enabled(menu_button, !reward_choice_required);
    button_row->add_child(menu_button);

    return view;
}

} // namespace defn