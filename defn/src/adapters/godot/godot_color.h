#ifndef GODOT_COLOR_H
#define GODOT_COLOR_H

#include "content_values.h"

#include <godot_cpp/variant/color.hpp>

namespace defn {

inline godot::Color to_godot_color(const Color &color) { return {color.r, color.g, color.b, color.a}; }

} // namespace defn

#endif
