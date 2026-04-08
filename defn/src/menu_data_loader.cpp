#include "menu_data_loader.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

std::optional<Dictionary> MenuDataLoader::load(const String &path) {
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (!file.is_valid()) {
        UtilityFunctions::printerr("MenuDataLoader: Failed to open: ", path);
        return std::nullopt;
    }

    const String json_text = file->get_as_text();
    file->close();

    Ref<JSON> json;
    json.instantiate();
    if (json->parse(json_text) != OK) {
        UtilityFunctions::printerr("MenuDataLoader: JSON parse error: ", json->get_error_message());
        return std::nullopt;
    }

    return json->get_data();
}

} // namespace defn