// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "deploy_tutorial_prompt.h"

#include "ui_theme_provider.h"
#include "ui_widgets.h"
#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <numbers>

namespace defn {
namespace {
constexpr double REVEAL_DELAY = 1.5;
constexpr double FADE_IN = 0.22;
constexpr double FADE_OUT = 0.3;
constexpr float ARROW_SCALE = 3;
constexpr float ARROW_HEIGHT = 12 + (30 * ARROW_SCALE);
constexpr float CARD_GAP = 12;
} // namespace

void DeployTutorialPrompt::_ready() {
    set_mouse_filter(MOUSE_FILTER_IGNORE);
    set_process_mode(PROCESS_MODE_PAUSABLE);
    set_process(false);
    hide();
    caption_ = make_surface("hud_tag");
    caption_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    add_child(caption_);
    auto *label = make_label("Click to deploy this unit", "body");
    label->set_name("DeployTutorialLabel");
    label->set_mouse_filter(MOUSE_FILTER_IGNORE);
    label->set_horizontal_alignment(godot::HORIZONTAL_ALIGNMENT_CENTER);
    label->add_theme_font_size_override("font_size", 2 * label->get_theme_font_size("font_size"));
    label->add_theme_color_override("font_color", UiThemeProvider::color("accent_strong"));
    caption_->add_child(label);
    caption_width_ = caption_->get_combined_minimum_size().x;
    label->set_autowrap_mode(godot::TextServer::AUTOWRAP_WORD_SMART);

    sound_ = memnew(godot::AudioStreamPlayer);
    sound_->set_name("TutorialPop");
    sound_->set_stream(godot::ResourceLoader::get_singleton()->load(SOUND_PATH));
    sound_->set_volume_db(-12);
    add_child(sound_);
}

void DeployTutorialPrompt::start(godot::Button *target) {
    cancel();
    target_ = target;
    elapsed_ = 0;
    fade_elapsed_ = 0;
    dismissing_ = false;
    set_process(target_ != nullptr);
}

void DeployTutorialPrompt::cancel() {
    hide();
    set_process(false);
    target_ = nullptr;
}

void DeployTutorialPrompt::dismiss() {
    if (target_ == nullptr || dismissing_) {
        return;
    }
    if (!is_visible()) {
        cancel();
        return;
    }
    dismissing_ = true;
    fade_alpha_ = get_modulate().a;
}

void DeployTutorialPrompt::_process(double delta) {
    if (target_ == nullptr) {
        return;
    }
    elapsed_ += delta;
    if (elapsed_ < REVEAL_DELAY) {
        return;
    }
    if (!is_visible()) {
        show();
        if (sound_->get_stream().is_valid()) {
            sound_->play();
        }
    }
    const double age = elapsed_ - REVEAL_DELAY;
    float alpha = static_cast<float>(std::clamp(age / FADE_IN, 0.0, 1.0));
    if (dismissing_) {
        fade_elapsed_ += delta;
        alpha = fade_alpha_ * static_cast<float>(std::max(0.0, 1.0 - (fade_elapsed_ / FADE_OUT)));
        if (fade_elapsed_ >= FADE_OUT) {
            cancel();
            return;
        }
    }
    set_modulate({1, 1, 1, alpha});
    position_above_card();
}

void DeployTutorialPrompt::position_above_card() {
    const auto *parent = get_parent_control();
    const auto inverse = parent->get_global_transform_with_canvas().affine_inverse();
    const auto target_top = inverse.xform(target_->get_global_transform_with_canvas().xform({target_->get_size().x / 2, 0}));
    caption_->set_custom_minimum_size({std::min(caption_width_, std::max(1.0F, parent->get_size().x - 16)), 0});
    const auto caption_size = caption_->get_combined_minimum_size();
    const float width = caption_size.x;
    const float height = caption_size.y + ARROW_HEIGHT;
    const float bob = 5.0F * static_cast<float>(1.0 - std::cos((elapsed_ - REVEAL_DELAY) * std::numbers::pi));
    const float left = std::clamp(target_top.x - (width / 2), 8.0F, std::max(8.0F, parent->get_size().x - width - 8));
    set_position({left, target_top.y - height - CARD_GAP - bob});
    set_size({width, height});
    caption_->set_size(caption_size);
    arrow_x_ = target_top.x - left;
    queue_redraw();
}

void DeployTutorialPrompt::_draw() {
    if (caption_ == nullptr) {
        return;
    }
    draw_set_transform({arrow_x_, caption_->get_size().y + 8}, 0, {ARROW_SCALE, ARROW_SCALE});
    const godot::PackedVector2Array arrow{{-5, 0}, {5, 0}, {5, 14}, {15, 14}, {0, 30}, {-15, 14}, {-5, 14}};
    draw_colored_polygon(arrow, UiThemeProvider::color("accent"));
    auto outline = arrow;
    outline.push_back(arrow[0]);
    draw_polyline(outline, UiThemeProvider::color("neutral_ink"), 2.0, true);
}
} // namespace defn
