#include "progression_presentation.h"

#include "progression_manager.h"

namespace defn {

String ProgressionPresentation::format_level_name(const String &level_id) { return level_id.replace("_", " ").capitalize(); }

String ProgressionPresentation::format_reward_title(const String &reward_source, const String &reward_level_id) {
    if (reward_source == "first_clear") {
        return vformat("FIRST CLEAR UPGRADE: %s", format_level_name(reward_level_id));
    }
    if (reward_source == "rescue") {
        return vformat("RESCUE DRAFT: %s", format_level_name(reward_level_id));
    }

    return "CHOOSE 1 UPGRADE";
}

String ProgressionPresentation::format_reward_subtitle(const String &reward_source, const String &reward_level_id) {
    if (reward_source == "first_clear") {
        return vformat("%s cleared for the first time.", format_level_name(reward_level_id));
    }
    if (reward_source == "rescue") {
        return vformat("Rescue points funded emergency support for %s.", format_level_name(reward_level_id));
    }

    return {};
}

PackedStringArray ProgressionPresentation::describe_new_unlocks(const ProgressionManager &progression, int old_score, int new_score) {
    PackedStringArray result;

    for (const auto &unlock : progression.get_level_unlock_data()) {
        if (old_score < unlock.score_required && new_score >= unlock.score_required && progression.is_level_unlocked(unlock.level_id)) {
            result.push_back(vformat("NEW UNLOCK: %s!", format_level_name(unlock.level_id)));
        }
    }

    return result;
}

String ProgressionPresentation::format_level_button_label(const ProgressionManager &progression, const LevelUnlock &level_unlock) {
    String label_text = format_level_name(level_unlock.level_id);
    if (!progression.is_level_unlocked(level_unlock.level_id)) {
        label_text += vformat(" (Score: %d needed)", level_unlock.score_required);
        return label_text;
    }

    if (progression.is_level_completed(level_unlock.level_id)) {
        label_text += vformat(" - Best: %d", progression.get_highest_level_score(level_unlock.level_id));
    }

    return label_text;
}

} // namespace defn