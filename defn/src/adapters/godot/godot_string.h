#ifndef GODOT_STRING_H
#define GODOT_STRING_H

#include <cstdint>
#include <string>
#include <string_view>

#include <godot_cpp/variant/variant.hpp>

namespace defn {

inline std::string to_std_string(const godot::String &value) { return value.utf8().get_data(); }

inline godot::String to_godot_string(std::string_view value) {
    if (value.empty()) {
        return {};
    }
    return godot::String::utf8(value.data(), static_cast<int64_t>(value.size()));
}

} // namespace defn

#endif