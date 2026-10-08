// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "match_presentation.h"
#include "data_paths.h"
#include "game_manager.h"
#include "godot_string.h"
#include "hud.h"
#include "ui_theme_provider.h"
#include "unit_data.h"
#include "unit_selection_controller.h"
#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {
namespace {
std::pair<float, float> protected_world_band(const UnitDataLoader &content) {
    const auto &rules = content.get_globals().gameplay_rules;
    float top = rules.belt_top_y;
    float bottom = rules.belt_bottom_y;
    for (const auto &unit : content.get_units()) {
        if (unit.role == UnitRole::STRUCTURE) {
            continue;
        }
        top = std::min(top, rules.belt_top_y + ((unit.health_bar_offset.y - 24.0F) * unit.scale));
        for (const auto &[name, animation] : unit.animations) {
            const auto path = to_godot_string(animation.path_template);
            const godot::Ref<godot::Texture2D> texture = godot::ResourceLoader::get_singleton()->load(path.contains("%") ? godot::vformat(path, 0) : path);
            if (texture.is_null()) {
                continue;
            }
            const float half_height = static_cast<float>(texture->get_height()) * 0.5F;
            top = std::min(top, rules.belt_top_y + ((animation.offset.y - half_height) * unit.scale));
            bottom = std::max(bottom, rules.belt_bottom_y + ((animation.offset.y + half_height) * unit.scale));
        }
    }
    return {std::clamp(top / rules.viewport_height, 0.0F, 1.0F), std::clamp(bottom / rules.viewport_height, 0.0F, 1.0F)};
}
godot::Rect2 protected_base_bounds(godot::Node2D *base) {
    if (auto *sprite = godot::Object::cast_to<godot::Sprite2D>(base->get_node_or_null("TowerSprite")); sprite != nullptr) {
        return sprite->get_global_transform().xform(sprite->get_rect());
    }
    return {};
}
} // namespace

void MatchPresentation::_ready() {
    ResponsiveUiRoot::_ready();
    UnitDataLoader units;
    units.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS);
    const auto &rules = units.get_globals().gameplay_rules;
    world_size_ = {.width = rules.viewport_width, .height = rules.viewport_height};
    if (std::abs((world_size_.width / world_size_.height) - (16.0F / 9.0F)) > 0.001F) {
        godot::UtilityFunctions::printerr("MatchPresentation: gameplay reference must be 16:9");
        return;
    }
    auto *backdrop = memnew(godot::ColorRect);
    backdrop->set_color(UiThemeProvider::color("backdrop"));
    backdrop->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    backdrop->set_mouse_filter(MOUSE_FILTER_IGNORE);
    add_child(backdrop);
    host_ = memnew(godot::SubViewportContainer);
    host_->set_name("BattlefieldHost");
    host_->set_stretch(true);
    host_->set_mouse_filter(MOUSE_FILTER_STOP);
    add_child(host_);
    world_ = memnew(godot::SubViewport);
    world_->set_name("Battlefield");
    world_->set_disable_3d(true);
    world_->set_as_audio_listener_2d(true);
    world_->set_size_2d_override({static_cast<int>(world_size_.width), static_cast<int>(world_size_.height)});
    world_->set_size_2d_override_stretch(true);
    host_->add_child(world_);
    ui_ = memnew(godot::Control);
    ui_->set_name("FullScreenUI");
    ui_->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    ui_->set_mouse_filter(MOUSE_FILTER_IGNORE);
    add_child(ui_);
    apply_layout();
    auto *game = memnew(GameManager);
    game->set_name("GameManager");
    game->configure_presentation(
        ui_, std::move(units),
        [this](HUD *hud, const UnitDataLoader &content, godot::Node2D *base, UnitSelectionController *selection) {
            configure_hud(hud, content, base, selection);
        },
        [this] { invalidate_layout(); });
    world_->add_child(game);
}
void MatchPresentation::configure_hud(HUD *hud, const UnitDataLoader &content, godot::Node2D *base, UnitSelectionController *selection) {
    hud_ = hud;
    selection_ = selection;
    const auto [top, bottom] = protected_world_band(content);
    protected_top_ = top;
    protected_bottom_ = bottom;
    sky_left_ = std::clamp(static_cast<float>(protected_base_bounds(base).get_end().x) / world_size_.width, 0.0F, 1.0F);
    notify_content(ui_);
    apply_layout();
    // The HUD and pause overlay are now mounted; measure their content on the next layout pass.
    invalidate_layout();
}
void MatchPresentation::apply_layout() {
    if (host_ == nullptr) {
        return;
    }
    auto local = display_;
    local.content = {.width = get_size().x, .height = get_size().y};
    local.safe_area_applied = true;
    auto data = UiThemeProvider::data().responsive;
    data.sky_bottom = std::min(data.sky_bottom, protected_top_);
    data.bottom_top = std::max(data.bottom_top, protected_bottom_);
    const auto &card_data = is_phone_landscape(local, data) ? UiThemeProvider::phone_landscape_data().responsive : data;
    const auto card = UiThemeProvider::profile() == UiProfile::Small ? card_data.small_card : card_data.standard_card;
    const float full_field_width = std::min(get_size().x, get_size().y * world_size_.width / world_size_.height);
    const float field_left = (get_size().x - full_field_width) / 2;
    const float metrics_width =
        get_size().y > get_size().x ? get_size().x - (2 * data.margin) : get_size().x - field_left - (full_field_width * sky_left_) - (2 * data.margin);
    const auto context = resolve_ui_context(local, data, UiThemeProvider::profile());
    const bool desktop = context.desktop_match;
    if (hud_ != nullptr) {
        auto theme_context = UiThemeContext::Default;
        if (desktop) {
            theme_context = UiThemeContext::DesktopMatch;
        } else if (context.phone_landscape) {
            theme_context = UiThemeContext::PhoneLandscape;
        }
        hud_->apply_appearance(UiThemeProvider::appearance(theme_context));
    }
    const auto measured = hud_ == nullptr || desktop ? HudLayoutMetrics{} : hud_->measure_layout(metrics_width);
    const auto roster = hud_ == nullptr ? 0 : hud_->roster_size();
    const auto layout = desktop ? resolve_desktop_match_layout(local, UiThemeProvider::desktop_match_data().responsive, world_size_, roster)
                                : resolve_match_layout(local, data, card, world_size_, roster, measured, sky_left_);
    host_->set_position({layout.battlefield.x, layout.battlefield.y});
    const float density = desktop ? std::max(1.0F, display_.render_density) : 1;
    host_->set_scale({1 / density, 1 / density});
    host_->set_size({layout.battlefield.width * density, layout.battlefield.height * density});
    if (hud_ != nullptr) {
        hud_->apply_layout(layout);
    }
    if (selection_ != nullptr) {
        selection_->set_presentation_scale(layout.battlefield.width / world_size_.width, display_.touch);
    }
}
} // namespace defn
