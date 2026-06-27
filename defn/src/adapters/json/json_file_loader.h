#ifndef JSON_FILE_LOADER_H
#define JSON_FILE_LOADER_H

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>

namespace defn {

using namespace godot;

class JsonFileLoader {
  public:
    JsonFileLoader() = delete;

    static std::optional<Dictionary> load_dictionary(const String &path, const String &context);
};

} // namespace defn

#endif