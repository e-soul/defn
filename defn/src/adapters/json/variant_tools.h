// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef VARIANT_TOOLS_H
#define VARIANT_TOOLS_H

#include "content_values.h"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace defn {

class VariantTools {
  public:
    VariantTools() = delete;

    static godot::real_t as_real(const godot::Variant &value) { return static_cast<godot::real_t>(value); }
    static float as_float(const godot::Variant &value) { return static_cast<float>(value); }
    static double as_double(const godot::Variant &value) { return static_cast<double>(value); }
    static int as_int(const godot::Variant &value) { return static_cast<int>(value); }
    static bool as_bool(const godot::Variant &value) { return static_cast<bool>(value); }

    static Color as_color(const godot::Array &values, const Color &fallback) {
        return values.size() < 3
                   ? fallback
                   : Color{.r = as_float(values[0]), .g = as_float(values[1]), .b = as_float(values[2]), .a = values.size() >= 4 ? as_float(values[3]) : 1.0F};
    }

    static Vector2 as_vector2(const godot::Array &values, const Vector2 &fallback) {
        return values.size() < 2 ? fallback : Vector2{.x = as_float(values[0]), .y = as_float(values[1])};
    }
};

} // namespace defn

#endif
