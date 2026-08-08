// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "unit_control_config_loader.h"

#include "json_file_loader.h"
#include "variant_tools.h"

#include <algorithm>
#include <cmath>

#include <godot_cpp/variant/array.hpp>

namespace defn {

using namespace godot;

namespace {

bool is_number(const Variant &value) { return value.get_type() == Variant::INT || value.get_type() == Variant::FLOAT; }

float read_positive(const Dictionary &source, const char *key, float fallback) {
    const Variant value = source.get(key, fallback);
    if (!is_number(value)) {
        return fallback;
    }
    const float parsed = VariantTools::as_float(value);
    return std::isfinite(parsed) && parsed > 0.0F ? parsed : fallback;
}

float read_non_negative(const Dictionary &source, const char *key, float fallback) {
    const Variant value = source.get(key, fallback);
    if (!is_number(value)) {
        return fallback;
    }
    const float parsed = VariantTools::as_float(value);
    return std::isfinite(parsed) && parsed >= 0.0F ? parsed : fallback;
}

int read_positive_int(const Dictionary &source, const char *key, int fallback, int maximum) {
    const Variant value = source.get(key, fallback);
    if (!is_number(value)) {
        return fallback;
    }
    const int parsed = VariantTools::as_int(value);
    return parsed > 0 ? std::min(parsed, maximum) : fallback;
}

Color parse_color(const Variant &value, const Color &fallback) {
    if (value.get_type() != Variant::ARRAY) {
        return fallback;
    }
    const Array channels = value;
    if (channels.size() < 3 || !is_number(channels[0]) || !is_number(channels[1]) || !is_number(channels[2]) ||
        (channels.size() >= 4 && !is_number(channels[3]))) {
        return fallback;
    }

    const float red = VariantTools::as_float(channels[0]);
    const float green = VariantTools::as_float(channels[1]);
    const float blue = VariantTools::as_float(channels[2]);
    const float alpha = channels.size() >= 4 ? VariantTools::as_float(channels[3]) : 1.0F;
    if (!std::isfinite(red) || !std::isfinite(green) || !std::isfinite(blue) || !std::isfinite(alpha)) {
        return fallback;
    }
    return {
        .r = std::clamp(red, 0.0F, 1.0F),
        .g = std::clamp(green, 0.0F, 1.0F),
        .b = std::clamp(blue, 0.0F, 1.0F),
        .a = std::clamp(alpha, 0.0F, 1.0F),
    };
}

void parse_radius(const Dictionary &source, float &radius_x, float &radius_y) {
    const Variant value = source.get("radius", Array());
    if (value.get_type() != Variant::ARRAY) {
        return;
    }
    const Array radius = value;
    if (radius.size() < 2 || !is_number(radius[0]) || !is_number(radius[1])) {
        return;
    }
    const float parsed_x = VariantTools::as_float(radius[0]);
    const float parsed_y = VariantTools::as_float(radius[1]);
    if (std::isfinite(parsed_x) && parsed_x > 0.0F && std::isfinite(parsed_y) && parsed_y > 0.0F) {
        radius_x = parsed_x;
        radius_y = parsed_y;
    }
}

GroundMarkerConfig parse_ground_marker(const Dictionary &source, GroundMarkerConfig marker) {
    parse_radius(source, marker.radius_x, marker.radius_y);
    marker.border_width = read_positive(source, "border_width", marker.border_width);
    marker.ground_offset_y = read_non_negative(source, "ground_offset_y", marker.ground_offset_y);
    marker.fill_color = parse_color(source.get("fill_color", Variant()), marker.fill_color);
    marker.border_color = parse_color(source.get("border_color", Variant()), marker.border_color);
    return marker;
}

DestinationMarkerConfig parse_destination_marker(const Dictionary &source, DestinationMarkerConfig marker) {
    parse_radius(source, marker.radius_x, marker.radius_y);
    marker.border_width = read_positive(source, "border_width", marker.border_width);
    marker.fill_color = parse_color(source.get("fill_color", Variant()), marker.fill_color);
    marker.border_color = parse_color(source.get("border_color", Variant()), marker.border_color);

    const Dictionary pulse = source.get("pulse", Dictionary());
    const float minimum_scale = read_positive(pulse, "minimum_scale", marker.minimum_scale);
    const float maximum_scale = read_positive(pulse, "maximum_scale", marker.maximum_scale);
    if (maximum_scale >= minimum_scale) {
        marker.minimum_scale = minimum_scale;
        marker.maximum_scale = maximum_scale;
    }
    marker.pulse_duration_seconds = read_positive(pulse, "duration_seconds", static_cast<float>(marker.pulse_duration_seconds));
    marker.pulse_count = read_positive_int(pulse, "count", marker.pulse_count, 100);
    return marker;
}

} // namespace

std::optional<UnitControlConfig> UnitControlConfigLoader::load(const String &path) {
    const auto data = JsonFileLoader::load_dictionary(path, "UnitControlConfigLoader");
    return data ? std::optional<UnitControlConfig>(load_from_data(*data)) : std::nullopt;
}

UnitControlConfig UnitControlConfigLoader::load_from_data(const Dictionary &data) {
    UnitControlConfig config;
    const Dictionary picking = data.get("picking", Dictionary());
    config.picking.fallback_radius = read_positive(picking, "fallback_radius", config.picking.fallback_radius);
    config.picking.max_candidates = read_positive_int(picking, "max_candidates", config.picking.max_candidates, 1024);
    config.selection_marker = parse_ground_marker(data.get("selection_marker", Dictionary()), config.selection_marker);
    config.hover_marker = parse_ground_marker(data.get("hover_marker", Dictionary()), config.hover_marker);
    config.destination_marker = parse_destination_marker(data.get("destination_marker", Dictionary()), config.destination_marker);

    const Dictionary reposition = data.get("reposition", Dictionary());
    config.reposition.arrival_epsilon = read_non_negative(reposition, "arrival_epsilon", config.reposition.arrival_epsilon);
    return config;
}

} // namespace defn
