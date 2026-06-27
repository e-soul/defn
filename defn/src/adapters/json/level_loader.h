#ifndef LEVEL_LOADER_H
#define LEVEL_LOADER_H

#include "level_definition.h"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>

namespace defn {

using namespace godot;

class LevelLoader {
  public:
    LevelLoader() = delete;

    static std::optional<LevelDefinition> load(const String &path);
    static std::optional<LevelDefinition> load_from_data(const Dictionary &data);
};

} // namespace defn

#endif