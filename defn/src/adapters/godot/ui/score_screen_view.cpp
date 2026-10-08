// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "score_screen_view.h"

#include "godot_string.h"
#include "ui_screen_scaffold.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"

#include <algorithm>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <cmath>
#include <string>

namespace defn {
using namespace godot;

namespace {

String number_text(size_t number) { return to_godot_string(std::to_string(number)); }

String card_name(const UpgradeCardViewModel &card) {
    String name = card.name.empty() ? String("Upgrade") : to_godot_string(card.name);
    if (card.owned_count > 1) {
        name += "  x" + String::num_int64(card.owned_count);
    }
    return name;
}

// Controls are laid out from measured text. Never let a Button's fixed card floor hide its children.
void place(Control *control, const Rect2 &rect) {
    control->set_position(rect.position);
    control->set_size(rect.size);
}
} // namespace

void ScoreScreenView::configure(const ScoreScreenModel &model, const ScoreScreenActions &actions, Layout layout) {
    layout_ = layout;
    model_ = model;
    presentation_ = ScoreScreenPresenter::build(model);
    actions_ = actions;
    set_name(layout_ == Layout::Mobile ? "MobileScoreScreen" : "DesktopScoreScreen");
    set_process_mode(PROCESS_MODE_ALWAYS);
    set_mouse_filter(MOUSE_FILTER_STOP);
    UiThemeProvider::apply_to(this);

    const auto chrome = build_screen(this, {.manual_content = true});
    panel_ = chrome.panel;
    panel_->set_name("ScorePanel");
    content_ = chrome.content;
    content_->set_name("ScoreContent");
    footer_ = memnew(Control);
    footer_->set_name("ScoreActions");
    footer_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    content_->add_child(footer_);
    footer_rule_ = memnew(ColorRect);
    footer_rule_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    footer_->add_child(footer_rule_);
    pager_ = memnew(Control);
    pager_->set_name("ScorePager");
    pager_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    content_->add_child(pager_);
    connect("resized", callable_mp(this, &ScoreScreenView::resized));
    layout_in_rect(Rect2({}, get_size().x > 0 && get_size().y > 0 ? get_size() : godot::Vector2(1280, 720)));
}
void ScoreScreenView::resized() { dirty_ = true; }
void ScoreScreenView::context_changed() {
    layout_ = context_.mobile_score ? Layout::Mobile : Layout::Desktop;
    set_name(layout_ == Layout::Mobile ? "MobileScoreScreen" : "DesktopScoreScreen");
    dirty_ = true;
}
void ScoreScreenView::_process(double /*delta*/) {
    if (dirty_ && panel_ != nullptr) {
        layout_in_rect(Rect2({}, get_size().x > 0 && get_size().y > 0 ? get_size() : last_rect_.size));
    }
}

ScoreScreenViewNodes ScoreScreenView::show(Node *parent, const ScoreScreenModel &model, const ScoreScreenActions &actions, const UiContext &context) {
    if (parent == nullptr) {
        return {};
    }
    auto *view = memnew(ScoreScreenView);
    parent->add_child(view);
    view->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    view->configure(model, actions, context.mobile_score ? Layout::Mobile : Layout::Desktop);
    view->set_ui_context(context);
    return {.overlay = view, .panel = view->panel()};
}

float ScoreScreenView::text_width(const String &text, int size) const {
    return get_theme_font("font", "Label")->get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, size).x;
}

float ScoreScreenView::text_height(const String &text, float width, int size) const {
    return wrapped_text_height(get_theme_font("font", "Label"), text, width, size);
}

void ScoreScreenView::resolve_style() {
    style_.clear();
    const std::string prefix = layout_ == Layout::Desktop ? "score_desktop_" : "score_mobile_";
    const auto &metrics = UiThemeProvider::data().metrics;
    for (const auto &[name, value] : metrics) {
        if (name.starts_with(prefix)) {
            style_.insert_or_assign(name.substr(prefix.size()), value);
        }
    }
    if (compact_desktop_) {
        const std::string compact = "score_desktop_compact_";
        for (const auto &[name, value] : metrics) {
            if (name.starts_with(compact)) {
                style_.insert_or_assign(name.substr(compact.size()), value);
            }
        }
    }
}
float ScoreScreenView::metric(std::string_view suffix, int fallback) const {
    const auto found = style_.find(suffix);
    return static_cast<float>(found == style_.end() ? fallback : found->second);
}

float ScoreScreenView::result_gap() const {
    if (layout_ == Layout::Desktop) {
        return metric("row_gap", 4);
    }
    return gap_ < 8 ? 2.0F : gap_;
}

String ScoreScreenView::upgrade_name(const UpgradeCardViewModel &card) const {
    return layout_ == Layout::Desktop ? to_godot_string(card.name) : card_name(card);
}

Label *ScoreScreenView::add_text(Node *parent, const String &text, const Rect2 &rect, int size, std::string_view color, bool right) {
    // Establish the wrapping width before adding text. Otherwise Godot first measures a
    // zero-width label and clamps its height to a character-per-line minimum.
    auto *label = make_label({});
    label->add_theme_font_size_override("font_size", size);
    label->add_theme_color_override("font_color", UiThemeProvider::color(color));
    label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    label->set_horizontal_alignment(right ? HORIZONTAL_ALIGNMENT_RIGHT : HORIZONTAL_ALIGNMENT_LEFT);
    label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
    parent->add_child(label);
    place_wrapped_label(label, text, rect);
    return label;
}

Button *ScoreScreenView::add_button(Node *parent, const Action &action, std::string_view variant) {
    const auto button_variant = action.primary ? "primary" : variant;
    Button *button = nullptr;
    if (action.id.empty()) {
        button = make_button(action.text, button_variant, action.pressed);
        parent->add_child(button);
    } else {
        auto &retained = action_buttons_[action.id];
        button = retained;
        if (button == nullptr) {
            button = make_button(action.text, button_variant);
            retained = button;
            button->set_meta("score_focus_id", to_godot_string(action.id));
            button->set_meta("score_retained_action", true);
            content_->add_child(button);
        }
        if (button->get_parent() != parent) {
            button->reparent(parent, false);
        }
        button->show();
        button->set_text(action.text);
        button->set_theme_type_variation(UiThemeProvider::button_variation(button_variant));
        auto &previous_callback = action_callbacks_[action.id];
        if (previous_callback != action.pressed) {
            if (previous_callback.is_valid()) {
                button->disconnect("pressed", previous_callback);
            }
            if (action.pressed.is_valid()) {
                button->connect("pressed", action.pressed);
            }
            previous_callback = action.pressed;
        }
    }
    button->remove_meta("ui_variant");
    button->set_custom_minimum_size({0, target_});
    button->add_theme_font_size_override("font_size", body_size_);
    button->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    for (const char *state : {"normal", "hover", "pressed", "disabled", "focus"}) {
        Ref<StyleBox> style = button->get_theme_stylebox(state, UiThemeProvider::button_variation(button_variant))->duplicate();
        style->set_content_margin(SIDE_LEFT, gap_);
        style->set_content_margin(SIDE_RIGHT, gap_);
        style->set_content_margin(SIDE_TOP, 4);
        style->set_content_margin(SIDE_BOTTOM, 4);
        button->add_theme_stylebox_override(state, style);
    }
    apply_enabled(button, action.enabled);
    return button;
}

void ScoreScreenView::clear_content() {
    for (const auto &[id, button] : action_buttons_) {
        if (button->get_parent() != content_) {
            button->reparent(content_, false);
        }
        button->hide();
    }
    footer_->hide();
    pager_->hide();
    // Retain chrome and semantic actions; measured page content is disposable.
    for (int i = content_->get_child_count() - 1; i >= 0; --i) {
        Node *child = content_->get_child(i);
        if (child == footer_ || child == pager_ || child->has_meta("score_retained_action")) {
            continue;
        }
        content_->remove_child(child);
        child->queue_free();
    }
}

void ScoreScreenView::layout_in_rect(const Rect2 &usable) {
    if (panel_ == nullptr) {
        return;
    }
    last_rect_ = usable;
    dirty_ = false;
    String focused_id;
    if (get_viewport() != nullptr) {
        auto *focused = get_viewport()->gui_get_focus_owner();
        if (focused != nullptr && content_->is_ancestor_of(focused)) {
            focused_id = focused->get_meta("score_focus_id", String());
        }
    }
    clear_content();
    if (layout_ == Layout::Desktop) {
        layout_desktop(usable);
    } else {
        compact_desktop_ = false;
        resolve_style();
        body_size_ = static_cast<int>(metric("body_size", 16));
        supporting_size_ = static_cast<int>(metric("supporting_size", 14));
        target_ = UiThemeProvider::data().responsive.touch_target;
        gap_ = usable.size.y < 280 ? static_cast<float>(UiThemeProvider::spacing("xs")) : static_cast<float>(UiThemeProvider::spacing("sm"));
        const float margin = metric("margin", 4);
        const float inset = metric("padding", 8);
        const float width = std::min(usable.size.x - (2 * margin), metric("max_width", 1000));
        const godot::Vector2 size(width, std::max(1.0F, usable.size.y - (2 * margin)));
        place(panel_, {usable.position + godot::Vector2((usable.size.x - width) / 2, margin), size});
        // The panel's theme margins are kept in step with the measured content rectangle.
        Ref<StyleBox> style = UiThemeProvider::surface("panel")->duplicate();
        for (const auto side : {SIDE_LEFT, SIDE_TOP, SIDE_RIGHT, SIDE_BOTTOM}) {
            style->set_content_margin(side, inset);
        }
        panel_->add_theme_stylebox_override("panel", style);
        const float content_width = std::max(1.0F, size.x - (2 * inset));
        const float content_height = std::max(1.0F, size.y - (2 * inset));
        const bool landscape = usable.size.x > usable.size.y && content_width >= 500;
        const float top = build_header(content_width, landscape) + gap_;
        std::vector<Action> footer;
        if (page_ == Page::Results) {
            footer = result_actions();
        } else {
            footer.push_back({.id = "back", .text = "Back", .pressed = callable_mp(this, &ScoreScreenView::open_page).bind(static_cast<int>(Page::Results))});
        }
        const float bottom = content_height - build_actions(content_width, content_height, footer) - gap_;
        if (page_ == Page::Results) {
            build_results(content_width, top, bottom, landscape);
        } else {
            build_upgrades(content_width, top, bottom, landscape);
        }
    }
    restore_focus(focused_id);
}

void ScoreScreenView::restore_focus(const String &identity) {
    if (!identity.is_empty()) {
        const auto restore = [&](const auto &self, Node *node) -> bool {
            if (auto *button = Object::cast_to<Button>(node); button != nullptr && button->get_meta("score_focus_id", String()) == Variant(identity) &&
                                                              button->is_visible_in_tree() && !button->is_disabled()) {
                button->grab_focus();
                return true;
            }
            for (int i = 0; i < node->get_child_count(); ++i) {
                if (self(self, node->get_child(i))) {
                    return true;
                }
            }
            return false;
        };
        restore(restore, content_);
    }
}

void ScoreScreenView::layout_desktop(const Rect2 &usable) {
    compact_desktop_ = usable.size.y < UiThemeProvider::metric("score_desktop_compact_height", 600) ||
                       usable.size.x < UiThemeProvider::metric("score_desktop_compact_width", 700);
    resolve_style();
    body_size_ = static_cast<int>(metric("body_size", 16));
    supporting_size_ = static_cast<int>(metric("supporting_size", 16));
    target_ = metric("target", 48);
    gap_ = metric("gap", 24);
    const float margin = metric("margin", 24);
    const float inset = metric("padding", 28);
    const float width = std::max(1.0F, std::min(usable.size.x - (2 * margin), metric("max_width", 860)));
    const float content_width = std::max(1.0F, width - (2 * inset));
    const bool landscape = content_width >= metric("two_column_width", 500);
    Ref<StyleBox> style = UiThemeProvider::surface("panel")->duplicate();
    for (const auto side : {SIDE_LEFT, SIDE_TOP, SIDE_RIGHT, SIDE_BOTTOM}) {
        style->set_content_margin(side, inset);
    }
    panel_->add_theme_stylebox_override("panel", style);
    const float top = build_header(content_width, landscape) + gap_;
    // Reserve the complete pager before measuring cards, so wrapped pager buttons cannot overlap a card.
    const auto footer = page_ == Page::Results
                            ? result_actions()
                            : std::vector<Action>{{.id = "previous", .text = "Previous"}, {.id = "next", .text = "Next"}, {.id = "back", .text = "Back"}};
    const float footer_height = build_actions(content_width, 0, footer);
    const float preferred = top + desktop_body_height(content_width, landscape) + gap_ + footer_height + (2 * inset);
    const float height = std::max(1.0F, std::min(preferred, usable.size.y - (2 * margin)));
    place(panel_, {usable.position + ((usable.size - godot::Vector2(width, height)) / 2), {width, height}});
    const float content_height = std::max(1.0F, height - (2 * inset));
    desktop_footer_bottom_ = content_height;
    place(Object::cast_to<Control>(content_->get_node<Control>("ScoreActions")), {0, content_height - footer_height, content_width, footer_height});
    const float bottom = content_height - footer_height - gap_;
    if (page_ == Page::Results) {
        build_results(content_width, top, bottom, landscape);
        auto *rule = memnew(ColorRect);
        rule->set_mouse_filter(MOUSE_FILTER_IGNORE);
        rule->set_color(UiThemeProvider::color("border"));
        content_->add_child(rule);
        place(rule, {0, top - (gap_ / 2), content_width, static_cast<float>(UiThemeProvider::shape("border_width"))});
    } else {
        build_upgrades(content_width, top, bottom, landscape);
    }
}

float ScoreScreenView::desktop_body_height(float width, bool wide) const {
    if (page_ == Page::Results) {
        const auto lines = result_lines(result_rows(), width, wide ? 2 : 1);
        float height = 0;
        for (const auto &line : lines) {
            height += line.height + (height > 0 ? result_gap() : 0);
        }
        return height;
    }
    const bool owned = page_ == Page::Owned;
    const auto &cards = owned ? model_.owned_upgrades : model_.reward.available_upgrades;
    const size_t columns = static_cast<size_t>(std::clamp(std::floor((width + gap_) / (metric("card_min_width", 200) + gap_)), 1.0F, 3.0F));
    const float card_width = (width - static_cast<float>(columns - 1) * gap_) / static_cast<float>(columns);
    float height = 0;
    for (const auto &card : cards) {
        height = std::max(height, card_height(card, card_width, true, owned));
    }
    const size_t rows = std::min<size_t>(2, (cards.size() + columns - 1) / columns);
    float total = (static_cast<float>(rows) * height) + (static_cast<float>(rows > 0 ? rows - 1 : 0) * gap_);
    if (!owned && !presentation_.reward_subtitle.empty()) {
        total += text_height(to_godot_string(presentation_.reward_subtitle), width, supporting_size_) + gap_;
    }
    return total;
}

float ScoreScreenView::build_header(float width, bool landscape) {
    if (page_ != Page::Results) {
        const String title = page_ == Page::Rewards ? to_godot_string(presentation_.reward_title) : String("YOUR UPGRADES");
        const int heading_size = static_cast<int>(metric("heading_size", 18));
        const String note = page_ == Page::Rewards ? String("Choose 1") : number_text(model_.owned_upgrades.size()) + " owned";
        const float count_width = layout_ == Layout::Desktop ? text_width(note, supporting_size_) : 64;
        const float height = text_height(title, width - count_width - gap_, heading_size);
        add_text(content_, title, {0, 0, width - count_width - gap_, height}, heading_size, "accent")->set_name("ScorePageTitle");
        add_text(content_, note, {width - count_width, 0, count_width, height}, supporting_size_, "text_secondary", true)->set_name("ScorePageCount");
        return height;
    }
    return build_result_header(width, landscape);
}

float ScoreScreenView::build_result_header(float width, bool landscape) {
    const int title_size = static_cast<int>(metric("title_size", 24));
    const String title = to_godot_string(presentation_.title);
    const float title_width = text_width(title, title_size);
    const float owned_width =
        model_.owned_upgrades.empty() ? 0 : text_width("Your upgrades (" + number_text(model_.owned_upgrades.size()) + ")", body_size_) + (2 * gap_);
    std::vector<std::pair<String, String>> totals;
    for (const auto &[name, value, kind] : presentation_.stat_rows) {
        if (kind != ScoreStatKind::Breakdown) {
            totals.emplace_back(to_godot_string(name), to_godot_string(value));
        }
    }
    const int score_size = static_cast<int>(metric(landscape ? "landscape_total_size" : "total_size", landscape ? 22 : 28));
    const int career_size = static_cast<int>(metric(landscape ? "landscape_career_size" : "career_size", landscape ? 16 : 18));
    float totals_width = 0;
    for (size_t i = 0; i < totals.size(); ++i) {
        totals_width += std::max(text_width(totals[i].first, supporting_size_), text_width(totals[i].second, i == 0 ? score_size : career_size)) + gap_;
    }
    const bool inline_totals = layout_ == Layout::Mobile && landscape && title_width + owned_width + totals_width + (3 * gap_) <= width;
    float cursor_y = 0;
    float header_height = std::max(target_, text_height(title, width, title_size));
    add_text(content_, title, {0, 0, title_width, header_height}, title_size, presentation_.victory ? "victory" : "defeat")->set_name("ScoreOutcome");
    float next_x = title_width + gap_;
    if (owned_width > 0) {
        auto *button = add_button(content_, {.id = "owned",
                                             .text = "Your upgrades (" + number_text(model_.owned_upgrades.size()) + ")",
                                             .pressed = callable_mp(this, &ScoreScreenView::open_page).bind(static_cast<int>(Page::Owned))});
        button->set_name("ScoreOwnedButton");
        const bool fits = title_width + gap_ + owned_width <= width;
        float button_left = 0;
        if (fits) {
            button_left = inline_totals ? next_x : width - owned_width;
        }
        place(button, {button_left, fits ? 0 : header_height + gap_, std::min(width, owned_width), target_});
        if (!fits) {
            header_height += gap_ + target_;
        }
        next_x += owned_width + gap_;
    }
    cursor_y = inline_totals ? 0 : header_height + gap_;
    const float total_start = inline_totals ? std::max(next_x, width - totals_width) : 0;
    const float totals_height = build_totals(totals, {total_start, cursor_y, width - total_start, 0}, inline_totals);
    return std::max(header_height, cursor_y + totals_height);
}

float ScoreScreenView::build_totals(const std::vector<std::pair<String, String>> &totals, const Rect2 &rect, bool inline_totals) {
    const int score_size = static_cast<int>(metric(inline_totals ? "landscape_total_size" : "total_size", inline_totals ? 22 : 28));
    const int career_size = static_cast<int>(metric(inline_totals ? "landscape_career_size" : "career_size", inline_totals ? 16 : 18));
    float totals_height = 0;
    for (size_t i = 0; i < totals.size(); ++i) {
        const bool right = i > 0;
        const int value_size = i == 0 ? score_size : career_size;
        const float cell_width =
            inline_totals ? std::max(text_width(totals[i].first, supporting_size_), text_width(totals[i].second, value_size)) : (rect.size.x - gap_) / 2;
        float left = rect.position.x + (static_cast<float>(i) * (cell_width + gap_));
        if (inline_totals) {
            left = right ? rect.get_end().x - cell_width : rect.position.x;
        }
        const float label_height = text_height(totals[i].first, cell_width, supporting_size_);
        const float value_height = text_height(totals[i].second, cell_width, i == 0 ? score_size : career_size);
        add_text(content_, totals[i].first, {left, rect.position.y, cell_width, label_height}, supporting_size_, "text_secondary", right);
        add_text(content_, totals[i].second, {left, rect.position.y + label_height, cell_width, value_height}, i == 0 ? score_size : career_size,
                 "text_primary", right);
        totals_height = std::max(totals_height, label_height + value_height);
    }
    return totals_height;
}

std::vector<ScoreScreenView::Action> ScoreScreenView::result_actions() {
    std::vector<Action> result;
    if (model_.reward.requires_selection()) {
        result.push_back({.id = "reward",
                          .text = "Choose upgrade",
                          .pressed = callable_mp(this, &ScoreScreenView::open_page).bind(static_cast<int>(Page::Rewards)),
                          .enabled = true,
                          .primary = true});
    } else if (presentation_.next_level_button_visible) {
        result.push_back(
            {.id = "next_level", .text = "Next Level", .pressed = actions_.on_next_level, .enabled = presentation_.next_level_button_enabled, .primary = true});
    }
    if (presentation_.endless_button_visible) {
        result.push_back({.id = "endless",
                          .text = to_godot_string(presentation_.endless_button_label),
                          .pressed = actions_.on_endless,
                          .enabled = presentation_.endless_button_enabled,
                          .primary = result.empty()});
    }
    if (presentation_.retry_button_visible) {
        result.push_back(
            {.id = "retry", .text = "Retry", .pressed = actions_.on_retry, .enabled = presentation_.retry_button_enabled, .primary = result.empty()});
    }
    result.push_back({.id = "campaign", .text = "Campaign", .pressed = actions_.on_campaign, .enabled = presentation_.campaign_button_enabled});
    return result;
}

float ScoreScreenView::build_actions(float width, float bottom, const std::vector<Action> &actions) {
    if (layout_ == Layout::Desktop) {
        auto ordered = actions;
        if (page_ == Page::Results) {
            std::ranges::reverse(ordered);
        }
        std::vector<float> widths;
        float used = 0;
        for (const auto &action : ordered) {
            const float button_width = std::min(width, text_width(action.text, body_size_) + (2 * gap_));
            widths.push_back(button_width);
            used += button_width + (used > 0 ? gap_ : 0);
        }
        float cursor_y = 0;
        float cursor_x = used <= width ? width - used : 0;
        float row_height = target_;
        auto *footer = footer_;
        footer->show();
        for (size_t i = 0; i < ordered.size(); ++i) {
            if (cursor_x > 0 && cursor_x + widths[i] > width) {
                cursor_x = 0;
                cursor_y += row_height + gap_;
                row_height = target_;
            }
            const float height = std::max(target_, text_height(ordered[i].text, widths[i] - (2 * gap_), body_size_) + metric("button_vertical_padding", 8));
            row_height = std::max(row_height, height);
            place(add_button(footer, ordered[i]), {cursor_x, cursor_y, widths[i], height});
            cursor_x += widths[i] + gap_;
        }
        const float height = cursor_y + row_height;
        place(footer, {0, bottom - height, width, height});
        auto *rule = footer_rule_;
        rule->show();
        rule->set_color(UiThemeProvider::color("border"));
        rule->set_mouse_filter(MOUSE_FILTER_IGNORE);
        place(rule, {0, -gap_ / 2, width, static_cast<float>(UiThemeProvider::shape("border_width"))});
        return height;
    }
    const size_t columns = width < 500 && actions.size() > 3 ? 2 : actions.size();
    const float button_width = (width - static_cast<float>(columns - 1) * gap_) / static_cast<float>(columns);
    float row_height = target_;
    for (const auto &action : actions) {
        row_height = std::max(row_height, text_height(action.text, button_width - (2 * gap_), body_size_) + 8);
    }
    const size_t rows = (actions.size() + columns - 1) / columns;
    const float height = (static_cast<float>(rows) * row_height) + (static_cast<float>(rows - 1) * gap_);
    auto *footer = footer_;
    footer->show();
    footer_rule_->hide();
    place(footer, {0, bottom - height, width, height});
    for (size_t i = 0; i < actions.size(); ++i) {
        auto *button = add_button(footer, actions[i]);
        const size_t row_index = i / columns;
        place(button, {static_cast<float>(i % columns) * (button_width + gap_), static_cast<float>(row_index) * (row_height + gap_), button_width, row_height});
    }
    return height;
}

void ScoreScreenView::reset_footer_buttons() {
    for (int i = footer_->get_child_count() - 1; i >= 0; --i) {
        if (auto *button = Object::cast_to<Button>(footer_->get_child(i)); button != nullptr) {
            button->hide();
        }
    }
}

float ScoreScreenView::build_pager(float width, float bottom) {
    if (page_starts_.size() < 2) {
        return 0;
    }
    const bool subpage = page_ != Page::Results;
    if (layout_ == Layout::Desktop && subpage) {
        reset_footer_buttons();
        auto *count = Object::cast_to<Label>(content_->get_node<Label>("ScorePageCount"));
        count->set_text(number_text(current_page_ + 1) + " / " + number_text(page_starts_.size()));
        build_actions(width, desktop_footer_bottom_,
                      {{.id = "previous", .text = "Previous", .pressed = callable_mp(this, &ScoreScreenView::turn_page).bind(-1), .enabled = current_page_ > 0},
                       {.id = "next",
                        .text = "Next",
                        .pressed = callable_mp(this, &ScoreScreenView::turn_page).bind(1),
                        .enabled = current_page_ + 1 < page_starts_.size()},
                       {.id = "back", .text = "Back", .pressed = callable_mp(this, &ScoreScreenView::open_page).bind(static_cast<int>(Page::Results))}});
        return 0;
    }
    if (subpage) {
        footer_->hide();
        auto *count = Object::cast_to<Label>(content_->get_node_or_null("ScorePageCount"));
        count->set_text(number_text(current_page_ + 1) + " / " + number_text(page_starts_.size()));
    }
    auto *pager = pager_;
    pager->show();
    place(pager, {0, subpage ? bottom + gap_ : bottom - target_, width, target_});
    auto *previous = add_button(
        pager, {.id = "previous", .text = "Previous", .pressed = callable_mp(this, &ScoreScreenView::turn_page).bind(-1), .enabled = current_page_ > 0});
    auto *next = add_button(
        pager,
        {.id = "next", .text = "Next", .pressed = callable_mp(this, &ScoreScreenView::turn_page).bind(1), .enabled = current_page_ + 1 < page_starts_.size()});
    const float button_width = subpage ? (width - (2 * gap_)) / 3 : std::max(target_, text_width("Previous", body_size_) + (2 * gap_));
    place(previous, {0, 0, button_width, target_});
    place(next, {width - button_width, 0, button_width, target_});
    if (subpage) {
        auto *back =
            add_button(pager, {.id = "back", .text = "Back", .pressed = callable_mp(this, &ScoreScreenView::open_page).bind(static_cast<int>(Page::Results))});
        place(back, {button_width + gap_, 0, button_width, target_});
        return 0;
    }
    auto *count = add_text(pager, number_text(current_page_ + 1) + " / " + number_text(page_starts_.size()),
                           {button_width, 0, width - (2 * button_width), target_}, supporting_size_, "text_secondary");
    count->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    return target_ + gap_;
}

std::vector<ScoreScreenView::ResultRow> ScoreScreenView::result_rows() const {
    std::vector<ResultRow> rows;
    for (const auto &[name, value, kind] : presentation_.stat_rows) {
        if (kind == ScoreStatKind::Breakdown) {
            rows.push_back({.name = to_godot_string(name), .value = to_godot_string(value)});
        }
    }
    for (const auto &notice : presentation_.new_unlocks) {
        rows.push_back({.name = to_godot_string(notice), .value = {}, .notice = true});
    }
    if (layout_ == Layout::Desktop && model_.reward.requires_selection()) {
        rows.push_back({.name = "Choose an upgrade to continue.", .notice = true, .hint = true});
    }
    return rows;
}

std::vector<ScoreScreenView::ResultLine> ScoreScreenView::result_lines(const std::vector<ResultRow> &rows, float width, size_t columns) const {
    std::vector<ResultLine> lines;
    const float row_width = (width - static_cast<float>(columns - 1) * (2 * gap_)) / static_cast<float>(columns);
    for (size_t i = 0; i < rows.size();) {
        const size_t count = rows[i].notice ? 1 : std::min(columns, rows.size() - i);
        const size_t actual_count = count == 2 && rows[i + 1].notice ? 1 : count;
        float height = 0;
        for (size_t column = 0; column < actual_count; ++column) {
            const auto &row = rows[i + column];
            const float value_width = std::min(row_width * 0.5F, text_width(row.value, body_size_));
            float padding = gap_ < 8 ? 0.0F : 4.0F;
            if (layout_ == Layout::Desktop) {
                padding = metric("row_padding", 16);
            }
            height = std::max(height, std::max(text_height(row.name, row.notice ? width : row_width - value_width - gap_, body_size_),
                                               row.notice ? 0 : text_height(row.value, value_width, body_size_)) +
                                          padding);
        }
        lines.push_back({.first = i, .count = actual_count, .height = height});
        i += actual_count;
    }
    return lines;
}

void ScoreScreenView::paginate_lines(const std::vector<ResultLine> &lines, float available) {
    page_starts_ = paginate_score_lines(lines, available, result_gap());
}

void ScoreScreenView::draw_result_row(const ResultRow &row, const Rect2 &rect) {
    const float value_width = std::min(rect.size.x * 0.5F, text_width(row.value, body_size_));
    add_text(content_, row.name, {rect.position, {row.notice ? rect.size.x : rect.size.x - value_width - gap_, rect.size.y}}, body_size_,
             row.notice && !row.hint ? "accent" : "text_secondary");
    if (row.notice) {
        return;
    }
    add_text(content_, row.value, {rect.position.x + rect.size.x - value_width, rect.position.y, value_width, rect.size.y}, body_size_, "text_primary", true);
    auto *rule = memnew(ColorRect);
    rule->set_mouse_filter(MOUSE_FILTER_IGNORE);
    rule->set_color(UiThemeProvider::color("border_muted"));
    content_->add_child(rule);
    place(rule, {rect.position.x, rect.position.y + rect.size.y - 1, rect.size.x, 1});
}

void ScoreScreenView::build_results(float width, float top, float bottom, bool landscape) {
    const auto rows = result_rows();
    const size_t columns = landscape ? 2 : 1;
    const float row_width = (width - static_cast<float>(columns - 1) * (2 * gap_)) / static_cast<float>(columns);
    const auto lines = result_lines(rows, width, columns);
    paginate_lines(lines, bottom - top);
    if (page_starts_.size() > 1) {
        paginate_lines(lines, bottom - top - target_ - gap_);
    }
    const size_t anchor = anchors_[static_cast<size_t>(page_)];
    current_page_ = score_page_for_anchor(page_starts_, anchor);
    build_pager(width, bottom);
    const size_t begin = page_starts_[current_page_];
    const size_t end = current_page_ + 1 < page_starts_.size() ? page_starts_[current_page_ + 1] : rows.size();
    float cursor_y = top;
    const float row_gap = result_gap();
    for (const auto &line : lines) {
        if (line.first < begin || line.first >= end) {
            continue;
        }
        for (size_t column = 0; column < line.count; ++column) {
            const auto &row = rows[line.first + column];
            const float left = static_cast<float>(column) * (row_width + (2 * gap_));
            draw_result_row(row, {left, cursor_y, row.notice ? width : row_width, line.height});
        }
        cursor_y += line.height + row_gap;
    }
}

float ScoreScreenView::card_height(const UpgradeCardViewModel &card, float width, bool vertical, bool owned) const {
    const float padding = metric("card_padding", 10);
    const float icon = metric("icon_size", 28);
    const float text_available = width - (2 * padding) - (vertical ? 0 : icon + gap_);
    const float text = text_height(upgrade_name(card), text_available, static_cast<int>(metric("card_name_size", body_size_))) + gap_ +
                       text_height(to_godot_string(card.description), text_available, body_size_);
    const float minimum = layout_ == Layout::Desktop && vertical ? metric(owned ? "owned_card_height" : "reward_card_height", 172) : target_;
    return std::max(minimum, (2 * padding) + (vertical ? icon + gap_ + text : std::max(icon, text)));
}

void ScoreScreenView::add_upgrade_card(const UpgradeCardViewModel &card, const Rect2 &rect, bool vertical, bool owned) {
    const bool selected = model_.reward.selected_upgrade.has_value() && model_.reward.selected_upgrade->id == card.id;
    auto *button = add_button(content_,
                              {.text = {},
                               .pressed = owned || !model_.reward.requires_selection()
                                              ? Callable()
                                              : callable_mp(this, &ScoreScreenView::select_upgrade).bind(to_godot_string(card.id))},
                              selected ? "card_selected" : "card");
    button->set_name((owned ? String("OwnedUpgrade_") : String("RewardUpgrade_")) + to_godot_string(card.id));
    button->set_meta("upgrade_id", to_godot_string(card.id));
    button->set_accessibility_name(upgrade_name(card));
    if (owned || !model_.reward.requires_selection()) {
        button->set_mouse_filter(MOUSE_FILTER_IGNORE);
        button->set_focus_mode(FOCUS_NONE);
    }
    button->set_meta("score_focus_id", to_godot_string("upgrade:" + card.id));
    place(button, rect);
    const float padding = metric("card_padding", 10);
    const float icon_size = metric("icon_size", 28);
    auto *icon = make_icon(card.icon.empty() ? "generic" : card.icon, icon_size);
    button->add_child(icon);
    place(icon, {vertical && layout_ == Layout::Mobile ? (rect.size.x - icon_size) / 2 : padding, vertical ? padding : (rect.size.y - icon_size) / 2, icon_size,
                 icon_size});
    if (owned && layout_ == Layout::Desktop) {
        const String count = "x" + String::num_int64(card.owned_count);
        const float count_width = text_width(count, supporting_size_);
        add_text(button, count, {rect.size.x - padding - count_width, padding, count_width, icon_size}, supporting_size_, "accent", true);
    }
    const float left = vertical ? padding : padding + icon_size + gap_;
    float cursor_y = vertical ? padding + icon_size + gap_ : padding;
    const float width = rect.size.x - left - padding;
    const int name_size = static_cast<int>(metric("card_name_size", body_size_));
    const float name_height = text_height(upgrade_name(card), width, name_size);
    add_text(button, upgrade_name(card), {left, cursor_y, width, name_height}, name_size);
    cursor_y += name_height + gap_;
    add_text(button, to_godot_string(card.description), {left, cursor_y, width, text_height(to_godot_string(card.description), width, body_size_)}, body_size_,
             "text_secondary");
}

void ScoreScreenView::build_upgrades(float width, float top, float bottom, bool landscape) {
    const bool owned = page_ == Page::Owned;
    const auto &cards = owned ? model_.owned_upgrades : model_.reward.available_upgrades;
    if (!owned && !presentation_.reward_subtitle.empty()) {
        const String subtitle = to_godot_string(presentation_.reward_subtitle);
        const float height = text_height(subtitle, width, supporting_size_);
        add_text(content_, subtitle, {0, top, width, height}, supporting_size_, "text_secondary");
        top += height + gap_;
    }
    const size_t landscape_columns = owned ? 2 : 3;
    size_t columns = landscape ? landscape_columns : 1;
    bool vertical = landscape && !owned;
    if (layout_ == Layout::Desktop) {
        columns = static_cast<size_t>(std::clamp(std::floor((width + gap_) / (metric("card_min_width", 200) + gap_)), 1.0F, 3.0F));
        vertical = true;
    }
    auto height_for = [&](size_t count, bool stack) {
        const float card_width = (width - static_cast<float>(count - 1) * gap_) / static_cast<float>(count);
        float height = 0;
        for (const auto &card : cards) {
            height = std::max(height, card_height(card, card_width, stack, owned));
        }
        return height;
    };
    float height = height_for(columns, vertical);
    if (vertical && height > bottom - top) {
        columns = 1;
        vertical = false;
        height = height_for(columns, vertical);
    }
    size_t capacity = score_grid_capacity(bottom - top, height, gap_, columns, layout_ == Layout::Desktop ? 2 : cards.size());
    // Fall back to a full-width row when even one multi-column card cannot fit.
    if (owned && columns > 1 && height > bottom - top) {
        columns = 1;
        height = height_for(columns, false);
        capacity = 1;
    }
    page_starts_ = paginate_score_grid(cards.size(), capacity);
    const size_t anchor = anchors_[static_cast<size_t>(page_)];
    current_page_ = score_page_for_anchor(page_starts_, anchor);
    build_pager(width, bottom);
    if (layout_ == Layout::Desktop && page_starts_.size() == 1) {
        reset_footer_buttons();
        build_actions(width, desktop_footer_bottom_,
                      {{.id = "back", .text = "Back", .pressed = callable_mp(this, &ScoreScreenView::open_page).bind(static_cast<int>(Page::Results))}});
    }
    const float card_width = (width - static_cast<float>(columns - 1) * gap_) / static_cast<float>(columns);
    const size_t first = page_starts_[current_page_];
    const size_t count = std::min(capacity, cards.size() - first);
    const size_t visible_rows = (count + columns - 1) / columns;
    if (!owned && vertical && visible_rows == 1) {
        height = std::max(height, bottom - top);
    }
    for (size_t i = 0; i < count; ++i) {
        const size_t row_index = i / columns;
        add_upgrade_card(cards[first + i],
                         {static_cast<float>(i % columns) * (card_width + gap_), top + (static_cast<float>(row_index) * (height + gap_)), card_width, height},
                         vertical, owned);
    }
}

void ScoreScreenView::open_page(int page) {
    page_ = static_cast<Page>(page);
    dirty_ = true;
}

void ScoreScreenView::turn_page(int direction) {
    const auto index = static_cast<int>(current_page_) + direction;
    if (index >= 0 && static_cast<size_t>(index) < page_starts_.size()) {
        anchors_[static_cast<size_t>(page_)] = page_starts_[static_cast<size_t>(index)];
        dirty_ = true;
    }
}

void ScoreScreenView::select_upgrade(const String &upgrade_id) {
    if (model_.reward.requires_selection() && actions_.on_select_upgrade.is_valid()) {
        dirty_ = true;
        actions_.on_select_upgrade.call(upgrade_id);
    }
}

} // namespace defn
