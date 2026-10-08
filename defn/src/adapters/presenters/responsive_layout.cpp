// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "responsive_layout.h"
#include <algorithm>
#include <cmath>

namespace defn {
TrayLayout resolve_tray_layout(UiSize available, UiCardGeometry card, float gap, float navigation, std::size_t count, bool tall, bool navigation_below,
                               bool center) {
    TrayLayout result;
    result.columns = tall ? std::max(1, static_cast<int>((available.width + gap) / (card.width + gap))) : std::max(1, static_cast<int>(count));
    const auto rows = (count + static_cast<std::size_t>(result.columns) - 1) / static_cast<std::size_t>(result.columns);
    const float extent = count == 0 ? 0 : (tall ? static_cast<float>(rows) * (card.height + gap) : static_cast<float>(count) * (card.width + gap)) - gap;
    result.overflow = extent > (tall ? available.height : available.width);
    const float nav = result.overflow ? navigation : 0;
    const bool below = tall || navigation_below;
    result.clip = {.x = below ? 0.0F : nav,
                   .y = 0,
                   .width = std::max(1.0F, available.width - (below ? 0 : 2 * nav)),
                   .height = std::max(1.0F, available.height - (below && result.overflow ? nav + gap : 0))};
    const float visible = tall ? result.clip.height : result.clip.width;
    result.max_scroll = std::max(0.0F, extent - visible);
    const float content_width =
        tall ? std::max(0.0F, (static_cast<float>(std::min(count, static_cast<std::size_t>(result.columns))) * (card.width + gap)) - gap) : extent;
    result.centered_offset = center && (tall || !result.overflow) ? std::max(0.0F, (result.clip.width - content_width) / 2) : 0;
    result.content = tall ? UiSize{.width = result.clip.width, .height = std::max(extent, result.clip.height)}
                          : UiSize{.width = std::max(extent, result.clip.width), .height = card.height};
    const float nav_height = below ? nav : card.height;
    result.previous = {.x = 0, .y = below ? available.height - nav : 0, .width = nav, .height = nav_height};
    result.next = {.x = available.width - nav, .y = result.previous.y, .width = nav, .height = nav_height};
    return result;
}

bool uses_mobile_score_layout(const DisplaySnapshot &display) { return display.touch && (!display.fine_pointer || display.primary_coarse_pointer); }
bool uses_mobile_menu_layout(const DisplaySnapshot &display) { return display.primary_coarse_pointer || (display.touch && !display.fine_pointer); }
UiContext resolve_ui_context(const DisplaySnapshot &display, const UiResponsiveData &data, UiProfile profile) {
    const auto area = usable_rect(display);
    auto local = display;
    local.content = {.width = area.width, .height = area.height};
    local.safe_area = {};
    local.safe_area_applied = true;
    const bool wide_pointer = display.fine_pointer && area.width >= data.wide_pointer_width && area.height >= data.wide_pointer_height;
    return {.display = local,
            .profile = profile,
            .desktop_match = display.fine_pointer && !display.touch && area.width >= area.height,
            .phone_landscape = is_phone_landscape(local, data),
            .mobile_score = uses_mobile_score_layout(local),
            .wide_campaign = wide_pointer || (area.width >= data.campaign_min_width && area.height >= data.campaign_min_height)};
}
DisplaySnapshot normalize_display_snapshot(DisplaySnapshot display) {
    const auto normalize = [](float value, float fallback, float minimum, float precision) {
        if (!std::isfinite(value)) {
            return fallback;
        }
        return std::round(std::max(minimum, value) * precision) / precision;
    };
    // Logical geometry has subpixel precision; discard platform jitter below 1/64 logical pixel.
    display.content.width = normalize(display.content.width, 1, 1, 64);
    display.content.height = normalize(display.content.height, 1, 1, 64);
    display.render_density = normalize(display.render_density, 1, 1, 1024);
    display.safe_area.left = normalize(display.safe_area.left, 0, 0, 64);
    display.safe_area.top = normalize(display.safe_area.top, 0, 0, 64);
    display.safe_area.right = normalize(display.safe_area.right, 0, 0, 64);
    display.safe_area.bottom = normalize(display.safe_area.bottom, 0, 0, 64);
    return display;
}

UiRect usable_rect(const DisplaySnapshot &display) {
    const UiInsets insets = display.safe_area_applied ? UiInsets{} : display.safe_area;
    return {.x = insets.left,
            .y = insets.top,
            .width = std::max(1.0F, display.content.width - insets.left - insets.right),
            .height = std::max(1.0F, display.content.height - insets.top - insets.bottom)};
}
UiProfile resolve_ui_profile(const DisplaySnapshot &display, const UiResponsiveData &data, UiProfile previous) {
    const auto usable = usable_rect(display);
    const float edge = std::min(usable.width, usable.height);
    if (display.fine_pointer && usable.width >= data.wide_pointer_width && usable.height >= data.wide_pointer_height) {
        return UiProfile::Standard;
    }
    if (previous == UiProfile::Small) {
        return edge >= data.small_short_edge + data.hysteresis ? UiProfile::Standard : UiProfile::Small;
    }
    return edge < data.small_short_edge - data.hysteresis ? UiProfile::Small : UiProfile::Standard;
}
UiThemeData resolve_ui_theme(const UiThemeData &source, UiProfile profile, bool touch) {
    UiThemeData theme = source;
    const bool small = profile == UiProfile::Small;
    if (small) {
        theme.typography = source.responsive.small_type;
        theme.metrics["hud_icon_size"] = static_cast<int>(source.responsive.compact_hud_icon_size);
        theme.metrics["hud_segment_width"] = static_cast<int>(source.responsive.compact_integrity_segment_width);
    }
    const auto card = small ? source.responsive.small_card : source.responsive.standard_card;
    theme.metrics["deploy_card_portrait_size"] = static_cast<int>(card.portrait);
    theme.metrics["deploy_card_width"] = static_cast<int>(card.width);
    theme.metrics["deploy_card_height"] = static_cast<int>(card.height);
    const int target = static_cast<int>(touch ? source.responsive.touch_target : source.responsive.pointer_target);
    for (auto &[name, button] : theme.buttons) {
        button.min_height = std::max(button.min_height, target);
        if (name.starts_with("deploy_card")) {
            button.min_width = static_cast<int>(card.width);
            button.min_height = static_cast<int>(card.height);
        }
    }
    theme.spacing.screen_margin = static_cast<int>(source.responsive.margin);
    return theme;
}
namespace {
UiRect fit(UiRect region, UiSize world) {
    const float scale = std::min(region.width / world.width, region.height / world.height);
    const float width = world.width * scale;
    const float height = world.height * scale;
    return {.x = region.x + ((region.width - width) / 2), .y = region.y + ((region.height - height) / 2), .width = width, .height = height};
}

MatchLayout phone_landscape_layout(const DisplaySnapshot &display, const UiResponsiveData &data, UiCardGeometry card, UiSize world, UiSize readings) {
    MatchLayout layout;
    layout.usable = usable_rect(display);
    layout.battlefield = fit(layout.usable, world);
    layout.hud = HudArrangement::SingleRow;
    layout.placement = TrayPlacement::Bottom;
    layout.center_cards = true;
    const auto area = layout.usable;
    const float edge = data.phone_edge_margin;
    const float pause_size = data.touch_target;
    const float available = std::max(1.0F, area.width - (2 * (edge + pause_size + data.gap)));
    layout.metrics_scale = std::min(1.0F, available / std::max(1.0F, readings.width));
    const float width = readings.width * layout.metrics_scale;
    layout.metrics = {.x = area.x + ((area.width - width) / 2), .y = area.y + edge, .width = width, .height = readings.height * layout.metrics_scale};
    layout.pause = {.x = area.x + area.width - edge - pause_size, .y = area.y + edge, .width = pause_size, .height = pause_size};
    layout.tray = {.x = area.x + edge, .y = area.y + area.height - edge - card.height, .width = area.width - (2 * edge), .height = card.height};
    return layout;
}
} // namespace
bool is_phone_landscape(const DisplaySnapshot &display, const UiResponsiveData &data) {
    const auto area = usable_rect(display);
    return (display.touch || !display.fine_pointer) && area.width > area.height && area.height < data.small_short_edge;
}
MatchLayout resolve_desktop_match_layout(const DisplaySnapshot &display, const UiResponsiveData &data, UiSize world, std::size_t /*roster_size*/) {
    MatchLayout layout;
    layout.usable = usable_rect(display);
    layout.battlefield = fit(layout.usable, world);
    layout.reference = world;
    layout.desktop_reference = true;
    layout.hud = HudArrangement::Instruments;
    layout.placement = TrayPlacement::Bottom;
    layout.center_cards = true;
    layout.metrics = {.x = data.margin, .y = data.margin, .width = world.width - (2 * data.margin), .height = 0};
    const auto card = data.standard_card;
    layout.tray = {.x = data.margin, .y = world.height - data.margin - card.height, .width = world.width - (2 * data.margin), .height = card.height};
    return layout;
}
MatchLayout resolve_match_layout(const DisplaySnapshot &display, const UiResponsiveData &data, UiCardGeometry card, UiSize world, std::size_t roster,
                                 HudLayoutMetrics measured, float sky_left) {
    if (is_phone_landscape(display, data)) {
        return phone_landscape_layout(display, data, card, world, measured.single_row);
    }
    MatchLayout layout;
    layout.usable = usable_rect(display);
    const auto area = layout.usable;
    const float margin = data.margin;
    const float width = std::max(1.0F, area.width - (2 * margin));
    const float tray_height = card.height + (2 * margin);
    const float metrics_height = std::max(data.metrics_height, measured.flow_height);
    const UiRect inner{.x = area.x + margin, .y = area.y + margin, .width = width, .height = std::max(1.0F, area.height - (2 * margin))};
    if (area.height > area.width) {
        layout.family = LayoutFamily::Tall;
        layout.center_cards = true;
        const float top = std::max(data.tall_metrics_height, measured.flow_height);
        layout.metrics = {.x = inner.x, .y = inner.y, .width = width, .height = top};
        auto field_area =
            UiRect{.x = inner.x, .y = inner.y + top + data.gap, .width = width, .height = std::max(1.0F, inner.height - top - tray_height - (2 * data.gap))};
        layout.battlefield = fit(field_area, world);
        layout.battlefield.y = field_area.y;
        const float tray_y = layout.battlefield.y + layout.battlefield.height + data.gap;
        layout.tray = {.x = inner.x, .y = tray_y, .width = width, .height = std::max(card.height, area.y + area.height - margin - tray_y)};
        layout.columns = std::max(1, static_cast<int>((width + data.gap) / (card.width + data.gap)));
        return layout;
    }
    layout.battlefield = fit(area, world);
    // The familiar desktop instruments stay visible, with centered deployment cards. Small typography
    // is independent of this composition: a wide pointer window is not a phone HUD.
    if (display.fine_pointer && area.width >= data.wide_pointer_width && area.height >= data.wide_pointer_height && measured.instruments_width > 0 &&
        measured.instruments_width <= width) {
        layout.hud = HudArrangement::Instruments;
        layout.center_cards = true;
        if (layout.battlefield.height * (1 - data.bottom_top) < tray_height) {
            layout.battlefield = fit({.x = area.x, .y = area.y, .width = area.width, .height = std::max(1.0F, area.height - tray_height)}, world);
            layout.tray = {.x = inner.x, .y = area.y + area.height - tray_height, .width = width, .height = card.height};
        } else {
            layout.placement = TrayPlacement::Bottom;
            layout.tray = {.x = inner.x, .y = area.y + area.height - tray_height, .width = width, .height = card.height};
        }
        layout.metrics = {.x = inner.x, .y = inner.y, .width = width, .height = measured.instruments_height};
        return layout;
    }
    // Short, very wide browser stages already have large natural side gutters. Put the original
    // readings there and keep a single horizontal card strip in the other gutter. Navigation goes
    // below the strip, leaving enough width for a complete card rather than two large arrow buttons.
    const float left_width = std::max(measured.primary_gutter.width, card.width) + (2 * margin);
    const float right_width = std::max(measured.secondary_gutter.width, card.width) + (2 * margin);
    const float gutter_tray_height = card.height + data.gap + data.tray_navigation;
    const float middle_width = area.width - left_width - right_width - (2 * data.gap);
    if (middle_width > 0 && measured.primary_gutter.height <= inner.height &&
        measured.secondary_gutter.height + data.gap + gutter_tray_height <= inner.height) {
        const auto gutter_field = fit({.x = area.x + left_width + data.gap, .y = area.y, .width = middle_width, .height = area.height}, world);
        if (gutter_field.height >= area.height * data.gutter_field_fraction) {
            layout.hud = HudArrangement::Gutters;
            layout.placement = TrayPlacement::Gutter;
            layout.battlefield = gutter_field;
            layout.metrics = {.x = inner.x, .y = inner.y, .width = gutter_field.x - inner.x - data.gap, .height = inner.height};
            const float right_x = gutter_field.x + gutter_field.width + data.gap;
            layout.details = {.x = right_x, .y = inner.y, .width = area.x + area.width - margin - right_x, .height = measured.secondary_gutter.height};
            layout.tray = {.x = right_x, .y = layout.details.y + layout.details.height + data.gap, .width = layout.details.width, .height = gutter_tray_height};
            layout.tray_navigation_below = true;
            return layout;
        }
    }
    const float gutter = area.y + area.height - layout.battlefield.y - layout.battlefield.height;
    const float sky_height = layout.battlefield.height * data.sky_bottom;
    layout.metrics = {.x = inner.x, .y = inner.y, .width = width, .height = metrics_height};
    if (gutter >= tray_height && layout.battlefield.y - area.y >= metrics_height + margin) {
        layout.tray = {.x = inner.x, .y = layout.battlefield.y + layout.battlefield.height + margin, .width = width, .height = card.height};
    } else if (sky_height >= metrics_height + card.height + (3 * margin)) {
        layout.placement = TrayPlacement::Sky;
        layout.metrics = {.x = layout.battlefield.x + (layout.battlefield.width * sky_left) + margin,
                          .y = layout.battlefield.y + margin,
                          .width = area.x + area.width - layout.battlefield.x - (layout.battlefield.width * sky_left) - (2 * margin),
                          .height = metrics_height};
        layout.tray = {.x = layout.metrics.x, .y = layout.metrics.y + metrics_height + data.gap, .width = layout.metrics.width, .height = card.height};
        // A spacious field can use the bottom only when the shared protected zone is clear.
        if (layout.battlefield.height * (1 - data.bottom_top) >= tray_height) {
            layout.placement = TrayPlacement::Bottom;
            layout.tray.y = layout.battlefield.y + layout.battlefield.height - tray_height;
        }
    } else {
        const auto dock_field = fit({.x = area.x, .y = area.y, .width = area.width, .height = std::max(1.0F, area.height - tray_height)}, world);
        if (metrics_height + (2 * margin) <= dock_field.height * data.sky_bottom) {
            layout.battlefield = dock_field;
            layout.metrics = {.x = dock_field.x + (dock_field.width * sky_left) + margin,
                              .y = dock_field.y + margin,
                              .width = area.x + area.width - dock_field.x - (dock_field.width * sky_left) - (2 * margin),
                              .height = metrics_height};
        } else {
            const UiRect field_area{.x = area.x,
                                    .y = area.y + metrics_height + margin,
                                    .width = area.width,
                                    .height = std::max(1.0F, area.height - metrics_height - tray_height - (2 * margin))};
            layout.battlefield = fit(field_area, world);
        }
        layout.tray = {.x = inner.x, .y = area.y + area.height - tray_height, .width = width, .height = card.height};
    }
    return layout;
}
} // namespace defn
