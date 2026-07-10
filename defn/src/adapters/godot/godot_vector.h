#ifndef GODOT_VECTOR_H
#define GODOT_VECTOR_H

#include "content_values.h"

#include <godot_cpp/variant/vector2.hpp>

namespace defn {

inline godot::Vector2 to_godot_vector(const Vector2 &vector) { return {vector.x, vector.y}; }

inline Vector2 to_vector(const godot::Vector2 &vector) { return {static_cast<float>(vector.x), static_cast<float>(vector.y)}; }

} // namespace defn

#endif