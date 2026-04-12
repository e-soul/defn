#ifndef MENU_DATA_LOADER_H
#define MENU_DATA_LOADER_H

#include "menu_models.h"

#include <godot_cpp/variant/string.hpp>

#include <optional>

namespace defn {

using namespace godot;

class MenuDataLoader {
  public:
    MenuDataLoader() = delete;

    static std::optional<MenuContentData> load(const String &path);
};

} // namespace defn

#endif