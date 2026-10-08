// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "mobile_campaign_view.h"

#include "campaign_preview_view.h"
#include "godot_string.h"
#include "ui_theme_provider.h"
#include "ui_widgets.h"

#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>
#include <cmath>

namespace defn {
using namespace godot;

namespace {
void place(Control *control, const Rect2 &rect) {
    control->set_position(rect.position);
    control->set_size(rect.size);
}

Label *add_label(Node *parent, const char *name) {
    auto *label = make_label({});
    label->set_name(name);
    label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
    parent->add_child(label);
    return label;
}

Panel *add_plate(Node *parent, const char *name, bool badge = false) {
    auto *panel = memnew(Panel);
    panel->set_name(name);
    panel->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    Ref<StyleBoxFlat> style = UiThemeProvider::surface("panel")->duplicate();
    style->set_shadow_size(0);
    style->set_border_width_all(badge ? 0 : 1);
    if (badge) {
        style->set_bg_color(UiThemeProvider::color("backdrop"));
        style->set_corner_radius_all(UiThemeProvider::shape("corner_sm"));
    }
    panel->add_theme_stylebox_override("panel", style);
    parent->add_child(panel);
    return panel;
}
} // namespace

size_t MobileCampaignView::item_count() const { return model_.missions.size() + (model_.endless.has_value() ? 1 : 0); }

void MobileCampaignView::configure(const CampaignMapViewModel &model, std::vector<Ref<Texture2D>> previews, const MobileCampaignActions &actions) {
    model_ = model;
    previews_ = std::move(previews);
    actions_ = actions;
    set_name("MobileCampaign");
    set_process_mode(PROCESS_MODE_ALWAYS);
    set_mouse_filter(MOUSE_FILTER_STOP);
    set_clip_contents(true);
    UiThemeProvider::apply_to(this);
    build_controls();
    const auto found = std::ranges::find(model_.missions, model_.initial_selected_level_id, &CampaignMissionViewModel::level_id);
    selected_ = found == model_.missions.end() ? 0 : static_cast<size_t>(found - model_.missions.begin());
    update_content();
}

Button *MobileCampaignView::add_button(const String &name, const String &text, const Callable &pressed, std::string_view variant) {
    auto *button = make_button(text, variant, pressed);
    button->set_name(name);
    button->remove_meta("ui_variant");
    button->set_custom_minimum_size({0, static_cast<float>(UiThemeProvider::data().responsive.touch_target)});
    button->add_theme_font_size_override("font_size", static_cast<int>(UiThemeProvider::metric("campaign_mobile_body_size", 14)));
    for (const char *state : {"normal", "hover", "pressed", "disabled", "focus"}) {
        // The button has no theme ancestor yet; copy from the campaign's explicit theme boundary.
        Ref<StyleBox> style = get_theme()->get_stylebox(state, UiThemeProvider::button_variation(variant))->duplicate();
        for (const auto side : {SIDE_LEFT, SIDE_TOP, SIDE_RIGHT, SIDE_BOTTOM}) {
            style->set_content_margin(side, static_cast<float>(UiThemeProvider::spacing("sm")));
        }
        button->add_theme_stylebox_override(state, style);
    }
    add_child(button);
    return button;
}

void MobileCampaignView::build_controls() {
    auto *background = memnew(ColorRect);
    background->set_color(UiThemeProvider::color("backdrop"));
    background->set_mouse_filter(MOUSE_FILTER_IGNORE);
    add_child(background);
    background->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    heading_ = add_label(this, "CampaignTitle");
    heading_->set_text(to_godot_string(model_.title.empty() ? "CAMPAIGN" : model_.title));
    secured_ = add_label(this, "SecuredCount");
    secured_->set_text(vformat("%d / %d secured", model_.completed_count, static_cast<uint64_t>(model_.missions.size())));
    secured_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    card_ = add_plate(this, "MissionCard");
    preview_ = memnew(CampaignPreviewView);
    preview_->set_name("MissionPreview");
    card_->add_child(preview_);
    operation_badge_ = add_plate(preview_, "OperationBadge", true);
    status_badge_ = add_plate(preview_, "StatusBadge", true);
    requirement_plate_ = add_plate(preview_, "UnlockPlate", true);
    operation_ = add_label(operation_badge_, "Operation");
    status_ = add_label(status_badge_, "Status");
    requirement_ = add_label(requirement_plate_, "UnlockRequirement");
    status_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    operation_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    title_ = add_label(card_, "MissionTitle");
    tagline_ = add_label(card_, "MissionTagline");
    const std::array names{"THREAT", "WAVES", "DURATION"};
    for (size_t i = 0; i < names.size(); ++i) {
        stat_names_[i] = add_label(card_, names[i]);
        stat_names_[i]->set_text(names[i]);
        stat_values_[i] = add_label(card_, (std::string(names[i]) + "Value").c_str());
    }
    for (size_t i = 0; i < item_count(); ++i) {
        auto *indicator = add_plate(this, ("Progress" + std::to_string(i)).c_str(), true);
        auto *label = add_label(indicator, "Number");
        label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        label->set_text(i < model_.missions.size() ? vformat("%02d", model_.missions[i].sequence_number) : String::utf8("∞"));
        auto *line = memnew(ColorRect);
        line->set_name("StateLine");
        line->set_mouse_filter(MOUSE_FILTER_IGNORE);
        indicator->add_child(line);
        indicators_.push_back(indicator);
    }
    previous_ = add_button("PreviousMission", "<", callable_mp(this, &MobileCampaignView::turn_page).bind(-1));
    previous_->set_accessibility_name("Previous mission");
    next_ = add_button("NextMission", ">", callable_mp(this, &MobileCampaignView::turn_page).bind(1));
    next_->set_accessibility_name("Next mission");
    back_ = add_button("CampaignBack", "BACK", actions_.back);
    deploy_ = add_button("CampaignDeploy", "DEPLOY", callable_mp(this, &MobileCampaignView::deploy), "primary");
    page_count_ = add_label(this, "MissionPageCount");
    page_count_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
}

void MobileCampaignView::select_level(const String &level_id) { select_item({.level_id = to_std_string(level_id)}); }
void MobileCampaignView::select_item(const CampaignSelection &selection) {
    if (selection.endless && model_.endless.has_value()) {
        selected_ = model_.missions.size();
        update_content();
        return;
    }
    const String level_id = to_godot_string(selection.level_id);
    const auto found = std::ranges::find(model_.missions, to_std_string(level_id), &CampaignMissionViewModel::level_id);
    if (found != model_.missions.end()) {
        const auto index = static_cast<size_t>(found - model_.missions.begin());
        if (selected_ != index) {
            selected_ = index;
            update_content();
        }
    }
}

void MobileCampaignView::update_content() {
    if (item_count() == 0) {
        deploy_->set_disabled(true);
        return;
    }
    const CampaignEndlessViewModel *endless_model = nullptr;
    if (model_.endless.has_value()) {
        endless_model = &model_.endless.value();
    }
    const bool endless = selected_ == model_.missions.size() && endless_model != nullptr;
    const auto *mission = endless ? nullptr : &model_.missions[selected_];
    const auto &preview = endless ? endless_model->preview : mission->preview;
    const bool locked = !endless && mission->state == CampaignNodeState::LOCKED;
    const auto presentation =
        endless ? CampaignItemPresentation{.status = "OPEN", .deployment = "BEGIN WATCH", .locked = false} : campaign_item_presentation(mission->state);
    operation_->set_text(endless ? "STANDING WATCH" : vformat("OPERATION %02d", mission->sequence_number));
    const String action = to_godot_string(std::string(presentation.deployment));
    status_->set_text(to_godot_string(std::string(presentation.status)));
    requirement_->set_text(locked ? to_godot_string(mission->unlock_requirement) : String());
    requirement_plate_->set_visible(locked);
    title_->set_text(to_godot_string(endless ? endless_model->title : mission->name).to_upper());
    tagline_->set_text(to_godot_string(endless ? endless_model->tagline : mission->tagline));
    stat_values_[0]->set_text(endless ? "Escalating" : to_godot_string(mission->threat_label));
    stat_values_[1]->set_text(endless ? "UNBOUNDED" : String::num_int64(mission->wave_count));
    stat_values_[2]->set_text(endless ? "UNTIL THE BASE FALLS" : to_godot_string(mission->duration_label));
    if (selected_ < previews_.size()) {
        preview_->configure(previews_[selected_], preview.focus_x, preview.focus_y, preview.dossier_zoom);
    }
    // Dim just the artwork, so status and the complete unlock instruction remain readable.
    if (auto *art = Object::cast_to<Control>(preview_->get_child(0)); art != nullptr) {
        const float value = locked ? UiThemeProvider::metric("campaign_mobile_locked_art_percent", 45) / 100 : 1;
        art->set_modulate(godot::Color(value, value, value));
    }
    deploy_->set_text(action);
    deploy_->set_disabled(locked);
    previous_->set_disabled(selected_ == 0);
    next_->set_disabled(selected_ + 1 == item_count());
    page_count_->set_text(endless ? "ENDLESS" : vformat("%02d / %02d", mission->sequence_number, static_cast<uint64_t>(model_.missions.size())));
    layout();
}

float MobileCampaignView::text_height(const String &text, float width, int size) const {
    return wrapped_text_height(get_theme_font("font", "Label"), text, width, size);
}

void MobileCampaignView::place_text(Label *label, const Rect2 &rect, int size, std::string_view color) {
    label->add_theme_font_size_override("font_size", size);
    label->add_theme_color_override("font_color", UiThemeProvider::color(color));
    place_wrapped_label(label, label->get_text(), rect);
}

void MobileCampaignView::layout() {
    if (card_ == nullptr || get_size().x <= 0 || get_size().y <= 0 || laying_out_ || item_count() == 0) {
        return;
    }
    laying_out_ = true;
    const bool landscape = get_size().x > get_size().y;
    const bool short_screen = landscape && get_size().y < UiThemeProvider::metric("campaign_mobile_short_height", 280);
    const auto gap = static_cast<float>(UiThemeProvider::spacing(short_screen ? "xs" : "sm"));
    const float margin = UiThemeProvider::metric(short_screen ? "campaign_mobile_short_margin" : "campaign_mobile_margin", short_screen ? 8 : 12);
    const auto target = static_cast<float>(UiThemeProvider::data().responsive.touch_target);
    const float max_width = UiThemeProvider::metric("campaign_mobile_max_width", 1000);
    const float width = std::min(get_size().x - (2 * margin), max_width);
    const float left = (get_size().x - width) / 2;
    const float header = UiThemeProvider::metric("campaign_mobile_header_height", 24);
    const float footer_y = get_size().y - margin - target;
    const float nav_y = landscape ? footer_y : footer_y - target - gap;
    const float top = margin + header + gap;
    const int supporting = static_cast<int>(UiThemeProvider::metric("campaign_mobile_supporting_size", 12));
    const int heading = static_cast<int>(UiThemeProvider::metric("campaign_mobile_heading_size", 18));
    place_text(heading_, {left, margin, width * 0.55F, header}, heading, "accent");
    place_text(secured_, {left + (width * 0.55F), margin, width * 0.45F, header}, supporting, "text_secondary");
    place(card_, {left, top, width, nav_y - gap - top});
    layout_briefing(landscape, short_screen, gap);
    layout_badges(gap, header, supporting);
    layout_navigation(landscape, gap, {left, footer_y, width, target});
    laying_out_ = false;
}

void MobileCampaignView::layout_briefing(bool landscape, bool short_screen, float gap) {
    const int body_size =
        static_cast<int>(UiThemeProvider::metric(short_screen ? "campaign_mobile_short_body_size" : "campaign_mobile_body_size", short_screen ? 13 : 14));
    const int title_size =
        static_cast<int>(UiThemeProvider::metric(short_screen ? "campaign_mobile_short_title_size" : "campaign_mobile_title_size", short_screen ? 18 : 22));
    const int supporting = static_cast<int>(UiThemeProvider::metric("campaign_mobile_supporting_size", 12));
    const float padding = UiThemeProvider::metric(short_screen ? "campaign_mobile_short_padding" : "campaign_mobile_padding", short_screen ? 8 : 16);
    const float image_width = landscape ? card_->get_size().x * 0.46F : card_->get_size().x;
    const float text_width = (landscape ? card_->get_size().x - image_width : card_->get_size().x) - (2 * padding);
    const float title_height = text_height(title_->get_text(), text_width, title_size);
    const float tagline_height = text_height(tagline_->get_text(), text_width, body_size);
    const float stat_width = (text_width - (2 * gap)) / 3;
    float values_height = 0;
    for (const auto *value : stat_values_) {
        values_height = std::max(values_height, text_height(value->get_text(), stat_width, body_size));
    }
    const float stat_label_height = text_height("DURATION", stat_width, supporting);
    const float briefing_height = title_height + tagline_height + stat_label_height + values_height + (3 * gap) + (2 * padding);
    const auto target = static_cast<float>(UiThemeProvider::data().responsive.touch_target);
    const float image_height = landscape ? card_->get_size().y : std::max(target, card_->get_size().y - briefing_height);
    place(preview_, {1, 1, image_width - 2, image_height - 2});
    const float text_left = (landscape ? image_width : 0) + padding;
    float row_y = (landscape ? std::max(0.0F, (card_->get_size().y - briefing_height) / 2) : image_height) + padding;
    place_text(title_, {text_left, row_y, text_width, title_height}, title_size);
    row_y += title_height + gap;
    place_text(tagline_, {text_left, row_y, text_width, tagline_height}, body_size);
    row_y += tagline_height + gap;
    for (size_t i = 0; i < stat_names_.size(); ++i) {
        const float column_x = text_left + (static_cast<float>(i) * (stat_width + gap));
        place_text(stat_names_[i], {column_x, row_y, stat_width, stat_label_height}, supporting, "text_secondary");
        place_text(stat_values_[i], {column_x, row_y + stat_label_height + gap, stat_width, values_height}, body_size);
    }
}

void MobileCampaignView::layout_badges(float gap, float header, int supporting) {
    const auto badge_padding = static_cast<float>(UiThemeProvider::spacing("sm"));
    const auto font = get_theme_font("font", "Label");
    const float operation_width = font->get_string_size(operation_->get_text(), HORIZONTAL_ALIGNMENT_LEFT, -1, supporting).x + (2 * badge_padding);
    const float status_width = font->get_string_size(status_->get_text(), HORIZONTAL_ALIGNMENT_LEFT, -1, supporting).x + (2 * badge_padding);
    place(operation_badge_, {gap, gap, operation_width, header});
    place(status_badge_, {preview_->get_size().x - gap - status_width, gap, status_width, header});
    place_text(operation_, {badge_padding, 0, operation_width - (2 * badge_padding), header}, supporting);
    std::string_view state_color = "accent";
    if (selected_ < model_.missions.size()) {
        const auto state = model_.missions[selected_].state;
        if (state == CampaignNodeState::COMPLETED) {
            state_color = "state_success";
        } else if (state == CampaignNodeState::LOCKED) {
            state_color = "text_secondary";
        }
    }
    place_text(status_, {badge_padding, 0, status_width - (2 * badge_padding), header}, supporting, state_color);
    const float unlock_width = preview_->get_size().x - (2 * gap);
    const float unlock_height = text_height(requirement_->get_text(), unlock_width - (2 * badge_padding), supporting) + (2 * gap);
    place(requirement_plate_, {gap, preview_->get_size().y - gap - unlock_height, unlock_width, unlock_height});
    place_text(requirement_, {badge_padding, gap, unlock_width - (2 * badge_padding), unlock_height - (2 * gap)}, supporting);
}

void MobileCampaignView::layout_navigation(bool landscape, float gap, const Rect2 &footer) {
    const float target = footer.size.y;
    const float width = footer.size.x;
    const float left = footer.position.x;
    const float nav_y = landscape ? footer.position.y : footer.position.y - target - gap;
    const int supporting = static_cast<int>(UiThemeProvider::metric("campaign_mobile_supporting_size", 12));
    const float action_width = landscape ? std::max(120.0F, width * 0.18F) : width * 0.4F;
    const float back_width = landscape ? std::max(76.0F, width * 0.11F) : width * 0.24F;
    const float nav_available = landscape ? width - action_width - back_width - (3 * gap) : width;
    const float indicator_width =
        std::min(UiThemeProvider::metric("campaign_mobile_indicator_width", 32), (nav_available - (2 * target) - (2 * gap)) / static_cast<float>(item_count()));
    const float nav_width = (2 * target) + (2 * gap) + (static_cast<float>(item_count()) * indicator_width);
    const float nav_left = left + ((nav_available - nav_width) / 2);
    place(previous_, {nav_left, nav_y, target, target});
    place(next_, {nav_left + nav_width - target, nav_y, target, target});
    for (size_t i = 0; i < indicators_.size(); ++i) {
        auto *indicator = indicators_[i];
        place(indicator, {nav_left + target + gap + (static_cast<float>(i) * indicator_width), nav_y, indicator_width, target});
        Ref<StyleBoxFlat> style = indicator->get_theme_stylebox("panel")->duplicate();
        style->set_bg_color(UiThemeProvider::color(i == selected_ ? "neutral_raised" : "backdrop"));
        indicator->add_theme_stylebox_override("panel", style);
        const bool secured = i < model_.missions.size() && model_.missions[i].state == CampaignNodeState::COMPLETED;
        std::string_view number_color = secured ? "state_success" : "text_secondary";
        std::string_view line_color = secured ? "state_success" : "neutral_line";
        if (i == selected_) {
            number_color = "accent";
            line_color = "accent";
        }
        auto *number = Object::cast_to<Label>(indicator->get_child(0));
        place_text(number, {0, 0, indicator_width, target - gap}, supporting, number_color);
        auto *line = Object::cast_to<ColorRect>(indicator->get_child(1));
        line->set_color(UiThemeProvider::color(line_color));
        place(line, {gap, target - gap, std::max(1.0F, indicator_width - (2 * gap)), 2});
    }
    place(back_, {landscape ? left + width - action_width - gap - back_width : left, footer.position.y, back_width, target});
    place(deploy_, {left + width - action_width, footer.position.y, action_width, target});
    page_count_->set_visible(!landscape);
    place_text(page_count_, {left + back_width + gap, footer.position.y, width - action_width - back_width - (2 * gap), target}, supporting, "text_secondary");
}

void MobileCampaignView::turn_page(int direction) {
    const auto index = static_cast<std::ptrdiff_t>(selected_) + direction;
    if (index < 0 || static_cast<size_t>(index) >= item_count()) {
        return;
    }
    if (actions_.select) {
        const auto selected = static_cast<size_t>(index);
        actions_.select(selected == model_.missions.size() ? CampaignSelection{.endless = true}
                                                           : CampaignSelection{.level_id = model_.missions[selected].level_id});
    }
}

void MobileCampaignView::deploy() {
    if (selected_ == model_.missions.size() && model_.endless.has_value()) {
        if (actions_.endless.is_valid()) {
            actions_.endless.call();
        }
    } else if (selected_ < model_.missions.size() && model_.missions[selected_].state != CampaignNodeState::LOCKED && actions_.deploy.is_valid()) {
        actions_.deploy.call();
    }
}

void MobileCampaignView::cancel_gesture() { gesture_ = false; }

void MobileCampaignView::_notification(int what) {
    if (what == NOTIFICATION_RESIZED || what == NOTIFICATION_THEME_CHANGED) {
        cancel_gesture();
        layout();
    } else if (what == NOTIFICATION_VISIBILITY_CHANGED || what == NOTIFICATION_WM_WINDOW_FOCUS_OUT || what == NOTIFICATION_EXIT_TREE) {
        cancel_gesture();
    }
}

void MobileCampaignView::_gui_input(const Ref<InputEvent> &event) {
    const auto *button = Object::cast_to<InputEventMouseButton>(event.ptr());
    if (button != nullptr && button->get_button_index() == MOUSE_BUTTON_LEFT && button->is_pressed() && card_ != nullptr &&
        card_->get_rect().has_point(button->get_position())) {
        gesture_ = true;
        gesture_start_ = button->get_position();
        accept_event();
    }
}

void MobileCampaignView::_input(const Ref<InputEvent> &event) {
    if (!is_visible_in_tree()) {
        return;
    }
    // Godot emulates a single mouse sequence for touch; handling both would advance twice.
    if (gesture_) {
        const auto *button = Object::cast_to<InputEventMouseButton>(event.ptr());
        if (button != nullptr && button->get_button_index() == MOUSE_BUTTON_LEFT && !button->is_pressed()) {
            const auto point = get_global_transform_with_canvas().affine_inverse().xform(button->get_position());
            const auto delta = point - gesture_start_;
            cancel_gesture();
            if (std::abs(delta.x) >= UiThemeProvider::metric("campaign_mobile_swipe_distance", 40) && std::abs(delta.x) > std::abs(delta.y)) {
                turn_page(delta.x < 0 ? 1 : -1);
            }
            get_viewport()->set_input_as_handled();
        }
    }
    if (const auto *key = Object::cast_to<InputEventKey>(event.ptr()); key != nullptr && key->is_pressed() && !key->is_echo()) {
        if (key->get_keycode() == KEY_LEFT || key->get_keycode() == KEY_RIGHT) {
            turn_page(key->get_keycode() == KEY_RIGHT ? 1 : -1);
            get_viewport()->set_input_as_handled();
        }
    }
}
} // namespace defn
