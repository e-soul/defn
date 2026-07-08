#include "score_screen_view.h"

#include "owned_upgrades_panel.h"
#include "score_screen_view_model.h"
#include "upgrade_card_presenter.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/viewport.hpp>

#include <algorithm>

namespace defn {

namespace {

constexpr real_t SCORE_SCREEN_WIDTH_RATIO = 0.8;
constexpr real_t SCORE_SCREEN_MIN_WIDTH = 600.0;
constexpr real_t SCORE_SCREEN_HORIZONTAL_MARGIN = 32.0;
constexpr real_t UPGRADE_SELECTION_LABEL_WIDTH = 240.0;

real_t get_score_screen_width(Node *parent) {
    if (parent == nullptr || parent->get_viewport() == nullptr) {
        return SCORE_SCREEN_MIN_WIDTH;
    }

    const real_t viewport_width = parent->get_viewport()->get_visible_rect().size.x;
    if (viewport_width <= 0.0) {
        return SCORE_SCREEN_MIN_WIDTH;
    }

    const real_t horizontal_margins = SCORE_SCREEN_HORIZONTAL_MARGIN * static_cast<real_t>(2);
    const real_t max_width = std::max<real_t>(static_cast<real_t>(0), viewport_width - horizontal_margins);
    const real_t target_width = viewport_width * SCORE_SCREEN_WIDTH_RATIO;
    return std::min(std::max(target_width, std::min(SCORE_SCREEN_MIN_WIDTH, max_width)), max_width);
}

String to_godot_string(const std::string &value) { return {value.c_str()}; }

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

void add_upgrade_selection(VBoxContainer *content, const ScoreScreenRewardModel &reward, const ScoreScreenViewModel &view_model,
                           const Callable &on_select_upgrade) {
    if (content == nullptr || (reward.available_upgrades.empty() && view_model.new_unlocks.empty())) {
        return;
    }

    const std::string selected_upgrade_id = reward.selected_upgrade.has_value() ? reward.selected_upgrade->id : std::string();

    add_spacer(content, 12);

    auto *selection_row = memnew(HBoxContainer);
    selection_row->set_alignment(BoxContainer::ALIGNMENT_BEGIN);
    selection_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    selection_row->add_theme_constant_override("separation", 24);
    content->add_child(selection_row);

    auto *label_column = memnew(VBoxContainer);
    label_column->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    label_column->set_custom_minimum_size(Vector2(UPGRADE_SELECTION_LABEL_WIDTH, 0));
    label_column->add_theme_constant_override("separation", 6);
    selection_row->add_child(label_column);

    auto add_selection_label = [label_column](const String &text, int font_size, const Color &font_color) {
        auto *label = memnew(Label);
        label->set_text(text);
        label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
        label->set_custom_minimum_size(Vector2(UPGRADE_SELECTION_LABEL_WIDTH, 0));
        label->add_theme_font_size_override("font_size", font_size);
        label->add_theme_color_override("font_color", font_color);
        label_column->add_child(label);
    };

    if (view_model.reward_available) {
        add_selection_label(to_godot_string(view_model.reward_title), 24, Color(1.0, 0.85, 0.3));

        if (!view_model.reward_subtitle.empty()) {
            add_selection_label(to_godot_string(view_model.reward_subtitle), 18, Color(0.84, 0.88, 0.95));
        }
    }

    for (const auto &new_unlock : view_model.new_unlocks) {
        add_selection_label(to_godot_string(new_unlock), 20, Color(1.0, 0.85, 0.3));
    }

    if (reward.available_upgrades.empty()) {
        return;
    }

    auto *card_row = memnew(HBoxContainer);
    card_row->set_alignment(BoxContainer::ALIGNMENT_BEGIN);
    card_row->add_theme_constant_override("separation", 12);
    selection_row->add_child(card_row);

    for (const auto &card : reward.available_upgrades) {
        const bool selected = !selected_upgrade_id.empty() && card.id == selected_upgrade_id;

        Callable pressed_action;
        if (on_select_upgrade.is_valid() && !card.id.empty()) {
            pressed_action = on_select_upgrade.bind(to_godot_string(card.id));
        }

        auto *card_button = UpgradeCardPresenter::create(card, selected, false, pressed_action);
        card_row->add_child(card_button);
    }
}

void add_owned_upgrades_section(VBoxContainer *content, const std::vector<UpgradeCardViewModel> &owned_upgrades) {
    if (content == nullptr || owned_upgrades.empty()) {
        return;
    }

    add_spacer(content, 12);

    auto *owned_row = memnew(HBoxContainer);
    owned_row->set_alignment(BoxContainer::ALIGNMENT_BEGIN);
    owned_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    owned_row->add_theme_constant_override("separation", 24);
    content->add_child(owned_row);

    auto *label_column = memnew(VBoxContainer);
    label_column->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    label_column->set_custom_minimum_size(Vector2(UPGRADE_SELECTION_LABEL_WIDTH, 0));
    label_column->add_theme_constant_override("separation", 6);
    owned_row->add_child(label_column);

    auto *section_title = memnew(Label);
    section_title->set_text("YOUR UPGRADES");
    section_title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    section_title->set_custom_minimum_size(Vector2(UPGRADE_SELECTION_LABEL_WIDTH, 0));
    section_title->add_theme_font_size_override("font_size", 24);
    section_title->add_theme_color_override("font_color", Color(0.7, 0.85, 1.0));
    label_column->add_child(section_title);

    OwnedUpgradesPanel::Options owned_panel_options;
    owned_panel_options.min_size = Vector2(0, 240);
    owned_panel_options.layout = OwnedUpgradesPanel::Layout::HorizontalStrip;

    auto *owned_panel = OwnedUpgradesPanel::build(owned_upgrades, owned_panel_options);
    owned_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    owned_row->add_child(owned_panel);
}

} // namespace

ScoreScreenViewNodes ScoreScreenView::show(Node *parent, const ScoreScreenModel &model, const ScoreScreenActions &actions) {
    if (parent == nullptr) {
        return {};
    }

    const ScoreScreenViewModel presentation = ScoreScreenPresenter::build(model);

    ScoreScreenViewNodes view;

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
    view.panel->set_custom_minimum_size(Vector2(get_score_screen_width(parent), 0));
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
    title->set_text(to_godot_string(presentation.title));
    title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    title->add_theme_font_size_override("font_size", 48);
    title->add_theme_color_override("font_color", presentation.victory ? Color(0.2, 1.0, 0.3) : Color(1.0, 0.2, 0.2));
    content->add_child(title);

    add_spacer(content, 12);
    const size_t score_start_index = presentation.victory ? 5 : 4;
    for (size_t index = 0; index < std::min(score_start_index, presentation.stat_rows.size()); ++index) {
        add_stat_row(content, to_godot_string(presentation.stat_rows[index].first), to_godot_string(presentation.stat_rows[index].second));
    }

    auto *separator = memnew(ColorRect);
    separator->set_custom_minimum_size(Vector2(0, 2));
    separator->set_color(Color(0.4, 0.4, 0.5));
    content->add_child(separator);

    for (size_t index = score_start_index; index < presentation.stat_rows.size(); ++index) {
        add_stat_row(content, to_godot_string(presentation.stat_rows[index].first), to_godot_string(presentation.stat_rows[index].second));
    }

    add_upgrade_selection(content, model.reward, presentation, actions.on_select_upgrade);

    add_owned_upgrades_section(content, model.owned_upgrades);

    add_spacer(content, 16);

    auto *button_row = memnew(HBoxContainer);
    button_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    button_row->add_theme_constant_override("separation", 16);
    content->add_child(button_row);

    if (presentation.next_level_button_visible) {
        auto *next_button = make_action_button("Next Level");
        connect_if_valid(next_button, actions.on_next_level);
        apply_button_enabled(next_button, presentation.next_level_button_enabled);
        button_row->add_child(next_button);
    }

    auto *retry_button = make_action_button("Retry");
    connect_if_valid(retry_button, actions.on_retry);
    apply_button_enabled(retry_button, presentation.retry_button_enabled);
    button_row->add_child(retry_button);

    auto *menu_button = make_action_button("Main Menu");
    connect_if_valid(menu_button, actions.on_main_menu);
    apply_button_enabled(menu_button, presentation.main_menu_button_enabled);
    button_row->add_child(menu_button);

    return view;
}

} // namespace defn