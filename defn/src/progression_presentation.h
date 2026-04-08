#ifndef PROGRESSION_PRESENTATION_H
#define PROGRESSION_PRESENTATION_H

#include "progression_catalog.h"

#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace defn {

using namespace godot;

class ProgressionManager;

class ProgressionPresentation {
  public:
    ProgressionPresentation() = delete;

    static PackedStringArray describe_new_unlocks(const ProgressionManager &progression, int old_score, int new_score);
    static String format_level_button_label(const ProgressionManager &progression, const LevelUnlock &level_unlock);
};

} // namespace defn

#endif