#ifndef MENU_DATA_LOADER_H
#define MENU_DATA_LOADER_H

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <optional>

namespace defn {

using namespace godot;

class MenuDataLoader {
  public:
    MenuDataLoader() = delete;

    static std::optional<Dictionary> load(const String &path);
};

} // namespace defn

#endif