#ifndef VARIANT_TOOLS_H
#define VARIANT_TOOLS_H

#include <godot_cpp/core/math.hpp>
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
};

} // namespace defn

#endif