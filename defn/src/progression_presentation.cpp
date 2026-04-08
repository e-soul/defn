#include "progression_presentation.h"

#include "progression_manager.h"

namespace defn {

namespace {

String format_level_name(const String &level_id) { return level_id.replace("_", " ").capitalize(); }

} // namespace

PackedStringArray ProgressionPresentation::describe_new_unlocks(const ProgressionManager &progression, int old_score, int new_score) {
    PackedStringArray result;

    for (const auto &unlock : progression.get_unit_unlock_data()) {
        if (old_score < unlock.score_required && new_score >= unlock.score_required) {
            result.push_back(vformat("NEW UNLOCK: %s unit!", unlock.unit_id.capitalize()));
        }
    }

    for (const auto &unlock : progression.get_level_unlock_data()) {
        if (old_score < unlock.score_required && new_score >= unlock.score_required) {
            result.push_back(vformat("NEW UNLOCK: %s!", format_level_name(unlock.level_id)));
        }
    }

    for (const auto &upgrade : progression.get_upgrade_data()) {
        if (old_score < upgrade.score_required && new_score >= upgrade.score_required) {
            if (upgrade.type == "starting_energy") {
                result.push_back(vformat("NEW UPGRADE: +%d starting energy!", static_cast<int>(upgrade.value)));
            } else if (upgrade.type == "energy_regen") {
                result.push_back(vformat("NEW UPGRADE: Energy regen %d/sec!", static_cast<int>(upgrade.value)));
            } else if (upgrade.type == "bounty_mult") {
                result.push_back(vformat(String::utf8("NEW UPGRADE: x%.1f bounty from kills!"), upgrade.value));
            }
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