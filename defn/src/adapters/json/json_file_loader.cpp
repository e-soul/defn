#include "json_file_loader.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace defn {

std::optional<Dictionary> JsonFileLoader::load_dictionary(const String &path, const String &context) {
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (!file.is_valid()) {
        UtilityFunctions::printerr(context, ": Failed to open: ", path);
        return std::nullopt;
    }

    const String json_text = file->get_as_text();
    file->close();

    Ref<JSON> json;
    json.instantiate();
    const Error err = json->parse(json_text);
    if (err != OK) {
        UtilityFunctions::printerr(context, ": JSON parse error in ", path, ": ", json->get_error_message());
        return std::nullopt;
    }

    return Dictionary(json->get_data());
}

} // namespace defn