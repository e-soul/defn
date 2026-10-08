// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "deploy_tray.h"
#include "deploy_card_presenter.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"
#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace defn {
void DeployTray::_ready() {
    set_process_mode(PROCESS_MODE_PAUSABLE);
    set_mouse_filter(MOUSE_FILTER_STOP);
    initialize_controls();
    if (appearance_.data == nullptr) {
        apply_appearance(UiThemeProvider::appearance(UiThemeContext::Default));
    }
}
void DeployTray::_notification(int what) {
    if (what == NOTIFICATION_PAUSED || what == NOTIFICATION_APPLICATION_FOCUS_OUT) {
        cancel_gesture();
    }
}
void DeployTray::initialize_controls() {
    if (clip_ == nullptr) {
        clip_ = memnew(godot::Control);
        clip_->set_name("TrayClip");
        clip_->set_clip_contents(true);
        clip_->set_mouse_filter(MOUSE_FILTER_IGNORE);
        add_child(clip_);
        cards_ = memnew(godot::Control);
        cards_->set_name("DeployCards");
        cards_->set_mouse_filter(MOUSE_FILTER_IGNORE);
        clip_->add_child(cards_);
        previous_ = make_button("<", "secondary", callable_mp(this, &DeployTray::scroll_previous));
        next_ = make_button(">", "secondary", callable_mp(this, &DeployTray::scroll_next));
        previous_->set_tooltip_text("Previous deploy cards");
        next_->set_tooltip_text("More deploy cards");
        add_child(previous_);
        add_child(next_);
        previous_->hide();
        next_->hide();
    }
}
godot::Button *DeployTray::add_card(const DeployCardViewModel &model) {
    initialize_controls();
    const auto nodes = DeployCardPresenter::create_nodes(model, {});
    auto *button = nodes.frame.button;
    button->remove_meta("ui_variant");
    button->set_meta("unit_id", godot::String(model.unit_id.c_str()));
    // The tray owns pointer gestures; the buttons still own keyboard/controller focus and activation.
    button->set_mouse_filter(MOUSE_FILTER_IGNORE);
    cards_->add_child(button);
    card_nodes_.push_back(nodes);
    style_card(nodes);
    button->connect("focus_entered", callable_mp(this, &DeployTray::focus_card).bind(button));
    position_cards();
    return button;
}
void DeployTray::clear_cards() {
    cancel_gesture();
    for (const auto &nodes : card_nodes_) {
        auto *button = nodes.frame.button;
        cards_->remove_child(button);
        button->queue_free();
    }
    card_nodes_.clear();
    position_cards();
}
void DeployTray::apply_layout(const MatchLayout &layout) {
    cancel_gesture();
    const int old_columns = geometry_.columns;
    const auto old_card = card_geometry();
    const auto old_gap = card_theme().responsive.gap;
    const int first = static_cast<int>(scroll_ / (layout_.family == LayoutFamily::Tall ? old_card.height + old_gap : old_card.width + old_gap)) * old_columns;
    const bool changed_family = layout.family != layout_.family || layout.hud != layout_.hud;
    layout_ = layout;
    const auto card = card_geometry();
    const auto &data = card_theme().responsive;
    set_position({layout.tray.x, layout.tray.y});
    set_size({layout.tray.width, layout.tray.height});
    if (changed_family) {
        const int new_columns =
            layout.family == LayoutFamily::Tall ? std::max(1, static_cast<int>((layout.tray.width + data.gap) / (card.width + data.gap))) : 1;
        const int first_row = first / new_columns;
        scroll_ = static_cast<float>(first_row) * (layout.family == LayoutFamily::Tall ? card.height + data.gap : card.width + data.gap);
    }
    position_cards();
}
void DeployTray::apply_appearance(const UiAppearance &appearance) {
    if (appearance_.data != nullptr && appearance_.context == appearance.context && appearance_.revision == appearance.revision) {
        return;
    }
    appearance_ = appearance;
    for (const auto &nodes : card_nodes_) {
        style_card(nodes);
    }
    if (clip_ != nullptr) {
        position_cards();
    }
}
const UiThemeData &DeployTray::card_theme() const { return *appearance_.data; }
UiCardGeometry DeployTray::card_geometry() const { return appearance_.card; }
void DeployTray::position_cards() {
    if (clip_ == nullptr || appearance_.data == nullptr) {
        return;
    }
    const auto &theme = card_theme();
    const auto card = card_geometry();
    card_hit_height_ = card.height;
    const float visible_height =
        layout_.desktop_reference ? card.height : std::min(card.height, static_cast<float>(theme.metric("deploy_card_inline_height", 44)));
    geometry_ = resolve_tray_layout({.width = get_size().x, .height = get_size().y}, card, theme.responsive.gap, theme.responsive.tray_navigation,
                                    card_nodes_.size(), layout_.family == LayoutFamily::Tall, layout_.tray_navigation_below, layout_.center_cards);
    clip_->set_position({geometry_.clip.x, geometry_.clip.y});
    clip_->set_size({geometry_.clip.width, geometry_.clip.height});
    cards_->set_size({geometry_.content.width, geometry_.content.height});
    max_scroll_ = geometry_.max_scroll;
    position_buttons(card, visible_height, theme.responsive.gap, geometry_.columns);
    auto place_navigation = [&](godot::Button *button, UiRect rect) {
        button->set_visible(geometry_.overflow);
        button->set_custom_minimum_size({rect.width, rect.height});
        button->set_position({rect.x, rect.y});
        button->set_size({rect.width, rect.height});
    };
    place_navigation(previous_, geometry_.previous);
    place_navigation(next_, geometry_.next);
    update_scroll();
}
void DeployTray::update_scroll() {
    scroll_ = std::clamp(scroll_, 0.0F, max_scroll_);
    cards_->set_position(layout_.family == LayoutFamily::Tall ? godot::Vector2(geometry_.centered_offset, -scroll_)
                                                              : godot::Vector2(geometry_.centered_offset - scroll_, 0));
    previous_->set_disabled(scroll_ <= 0);
    next_->set_disabled(scroll_ >= max_scroll_);
}
void DeployTray::position_buttons(UiCardGeometry card, float visible_height, float gap, int columns) {
    const bool tall = layout_.family == LayoutFamily::Tall;
    // Tall overflow uses a narrower column count after navigation consumes its reserved geometry.
    const int tall_columns = tall ? std::max(1, static_cast<int>((clip_->get_size().x + gap) / (card.width + gap))) : columns;
    for (std::size_t i = 0; i < card_nodes_.size(); ++i) {
        auto *button = card_nodes_[i].frame.button;
        button->set_custom_minimum_size({card.width, visible_height});
        button->set_size({card.width, visible_height});
        const int col = static_cast<int>(i) % tall_columns;
        const int row = tall ? static_cast<int>(i) / tall_columns : 0;
        button->set_position({static_cast<float>(col) * (card.width + gap), (static_cast<float>(row) * (card.height + gap)) + card.height - visible_height});
    }
}
void DeployTray::style_card(const DeployCardNodes &nodes) const {
    auto *button = nodes.frame.button;
    const auto &theme = card_theme();
    const auto card = card_geometry();
    button->set_theme(appearance_.theme);
    if (auto *portrait = nodes.portrait; portrait != nullptr) {
        portrait->set_custom_minimum_size({card.portrait, card.portrait});
    }
    if (auto *cost = nodes.cost; cost != nullptr) {
        auto *parent = appearance_.context == UiThemeContext::DesktopMatch ? static_cast<godot::Node *>(nodes.frame.text) : nodes.frame.body;
        if (cost->get_parent() != parent) {
            cost->reparent(parent);
        }
        cost->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
        if (auto *icon = nodes.cost_icon; icon != nullptr) {
            const auto size = static_cast<float>(theme.metric("card_icon_size", 20));
            icon->set_custom_minimum_size({size, size});
        }
    }
    style_card_text(nodes, card, theme);
}
void DeployTray::style_card_text(const DeployCardNodes &nodes, UiCardGeometry card, const UiThemeData &theme) const {
    const int inset = appearance_.context == UiThemeContext::DesktopMatch ? theme.spacing.sm : theme.spacing.xs;
    const int right = appearance_.context == UiThemeContext::DesktopMatch ? inset : theme.spacing.md;
    if (auto *margins = nodes.frame.margins; margins != nullptr) {
        for (const char *side : {"margin_left", "margin_top", "margin_bottom"}) {
            margins->add_theme_constant_override(side, inset);
        }
        margins->add_theme_constant_override("margin_right", right);
    }
    if (auto *body = nodes.frame.body; body != nullptr) {
        body->add_theme_constant_override("separation", inset);
        body->set_alignment(godot::BoxContainer::ALIGNMENT_CENTER);
    }
    auto *text = nodes.frame.text;
    auto *cost = nodes.cost;
    if (text == nullptr || cost == nullptr) {
        return;
    }
    text->add_theme_constant_override("separation", theme.metric("deploy_card_text_gap", 0));
    text->set_h_size_flags(appearance_.context == UiThemeContext::DesktopMatch ? Control::SIZE_EXPAND_FILL : Control::SIZE_SHRINK_BEGIN);
    if (auto *title = nodes.title; title != nullptr) {
        float width = 0;
        if (appearance_.context != UiThemeContext::DesktopMatch) {
            const auto font = title->get_theme_font("font");
            const float preferred =
                std::ceil(font->get_string_size(title->get_text(), godot::HORIZONTAL_ALIGNMENT_LEFT, -1, title->get_theme_font_size("font_size")).x);
            const float available = card.width - card.portrait - static_cast<float>((3 * inset) + right) - cost->get_combined_minimum_size().x;
            width = std::clamp(preferred, 0.0F, std::max(0.0F, available));
        }
        title->set_custom_minimum_size({width, 0});
    }
}
void DeployTray::scroll_previous() {
    scroll_ -= layout_.family == LayoutFamily::Tall ? get_size().y : clip_->get_size().x;
    update_scroll();
}
void DeployTray::focus_card(godot::Button *card) {
    if (gesture_) {
        return;
    }
    const bool tall = layout_.family == LayoutFamily::Tall;
    const float start = tall ? card->get_position().y : card->get_position().x;
    const float end = start + (tall ? card->get_size().y : card->get_size().x);
    const float visible = tall ? clip_->get_size().y : clip_->get_size().x;
    if (start < scroll_) {
        scroll_ = start;
    } else if (end > scroll_ + visible) {
        scroll_ = end - visible;
    }
    update_scroll();
}
void DeployTray::scroll_next() {
    scroll_ += layout_.family == LayoutFamily::Tall ? get_size().y : clip_->get_size().x;
    update_scroll();
}
void DeployTray::cancel_gesture() {
    if (pressed_ != nullptr) {
        pressed_->set_pressed_no_signal(false);
    }
    pressed_ = nullptr;
    gesture_ = false;
    dragged_ = false;
}
void DeployTray::_gui_input(const godot::Ref<godot::InputEvent> &event) {
    if (auto *button = godot::Object::cast_to<godot::InputEventMouseButton>(event.ptr()); button != nullptr) {
        if (button->is_pressed() &&
            (button->get_button_index() == godot::MOUSE_BUTTON_WHEEL_DOWN || button->get_button_index() == godot::MOUSE_BUTTON_WHEEL_UP)) {
            if (button->get_button_index() == godot::MOUSE_BUTTON_WHEEL_DOWN) {
                scroll_next();
            } else {
                scroll_previous();
            }
        } else if (button->get_button_index() == godot::MOUSE_BUTTON_LEFT && button->is_pressed()) {
            cancel_gesture();
            gesture_ = true;
            start_ = button->get_position();
            start_scroll_ = scroll_;
            for (const auto &nodes : card_nodes_) {
                auto *card = nodes.frame.button;
                const godot::Vector2 point = start_ - clip_->get_position() - cards_->get_position();
                const auto hit = card->get_rect().grow_individual(0, card_hit_height_ - card->get_size().y, 0, 0);
                if (hit.has_point(point) && !card->is_disabled()) {
                    pressed_ = card;
                    // Pointer clicks and taps do not acquire keyboard focus.
                    card->set_pressed_no_signal(true);
                    break;
                }
            }
        }
    }
    accept_event();
}
void DeployTray::_input(const godot::Ref<godot::InputEvent> &event) {
    if (!gesture_) {
        return;
    }
    if (auto *motion = godot::Object::cast_to<godot::InputEventMouseMotion>(event.ptr()); motion != nullptr) {
        handle_gesture(event, get_global_transform_with_canvas().affine_inverse().xform(motion->get_position()));
        get_viewport()->set_input_as_handled();
    } else if (auto *button = godot::Object::cast_to<godot::InputEventMouseButton>(event.ptr());
               button != nullptr && button->get_button_index() == godot::MOUSE_BUTTON_LEFT && !button->is_pressed()) {
        handle_gesture(event, get_global_transform_with_canvas().affine_inverse().xform(button->get_position()));
        get_viewport()->set_input_as_handled();
    }
}
void DeployTray::handle_gesture(const godot::Ref<godot::InputEvent> &event, godot::Vector2 point) {
    const auto displacement = point - start_;
    if (displacement.length() > appearance_.data->responsive.gesture_slop) {
        dragged_ = true;
    }
    if (dragged_) {
        if (pressed_ != nullptr) {
            pressed_->set_pressed_no_signal(false);
        }
        scroll_ = start_scroll_ - (layout_.family == LayoutFamily::Tall ? displacement.y : displacement.x);
        update_scroll();
    }
    if (godot::Object::cast_to<godot::InputEventMouseButton>(event.ptr()) != nullptr) {
        auto *card = pressed_;
        const bool deploy = !dragged_ && card != nullptr && !card->is_disabled() && godot::Rect2({}, get_size()).has_point(point);
        cancel_gesture();
        if (deploy) {
            card->emit_signal("pressed");
        }
    }
}
} // namespace defn
