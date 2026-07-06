#ifndef PROGRESSION_PRESENTATION_H
#define PROGRESSION_PRESENTATION_H

#include "progression_catalog.h"
#include "progression_service.h"
#include "score_screen_models.h"
#include "upgrade_catalog.h"

#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace defn {

using namespace godot;

class ProgressionPresentation {
  public:
    ProgressionPresentation() = delete;

    static String format_level_name(const String &level_id);
    static String format_reward_title(const String &reward_source, const String &reward_level_id);
    static String format_reward_subtitle(const String &reward_source, const String &reward_level_id);
    static PackedStringArray describe_new_unlocks(const ProgressionService &progression, bool victory, const String &completed_level_id);
    static String format_level_button_label(const ProgressionService &progression, const LevelUnlock &level_unlock);
    static UpgradeCardViewModel build_upgrade_card_view_model(const UpgradeCardDefinition &card);
};

} // namespace defn

#endif