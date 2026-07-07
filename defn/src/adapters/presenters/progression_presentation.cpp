#include "progression_presentation.h"

#include <algorithm>
#include <cctype>

namespace defn {

namespace {

const ProgressionUpgradePresentation *find_presentation(const std::vector<ProgressionUpgradePresentation> &presentations, const std::string &upgrade_id) {
    for (const auto &presentation : presentations) {
        if (presentation.id == upgrade_id) {
            return &presentation;
        }
    }
    return nullptr;
}

} // namespace

std::string ProgressionPresentation::format_level_name(const std::string &level_id) {
    std::string result = level_id;
    bool capitalize_next = true;
    for (char &character : result) {
        if (character == '_') {
            character = ' ';
            capitalize_next = true;
            continue;
        }

        const auto unsigned_character = static_cast<unsigned char>(character);
        if (capitalize_next) {
            character = static_cast<char>(std::toupper(unsigned_character));
            capitalize_next = false;
        } else {
            character = static_cast<char>(std::tolower(unsigned_character));
        }
    }
    return result;
}

std::string ProgressionPresentation::format_reward_title(ProgressionRewardSource reward_source, const std::string &reward_level_id) {
    if (reward_source == ProgressionRewardSource::FIRST_CLEAR) {
        return "FIRST CLEAR UPGRADE: " + format_level_name(reward_level_id);
    }
    if (reward_source == ProgressionRewardSource::RESCUE) {
        return "RESCUE DRAFT: " + format_level_name(reward_level_id);
    }

    return "CHOOSE 1 UPGRADE";
}

std::string ProgressionPresentation::format_reward_subtitle(ProgressionRewardSource reward_source, const std::string &reward_level_id) {
    if (reward_source == ProgressionRewardSource::FIRST_CLEAR) {
        return format_level_name(reward_level_id) + " cleared for the first time.";
    }
    if (reward_source == ProgressionRewardSource::RESCUE) {
        return "Career progress unlocked emergency support for " + format_level_name(reward_level_id) + ".";
    }

    return {};
}

std::vector<std::string> ProgressionPresentation::describe_new_unlocks(const std::vector<std::string> &unlocked_level_ids) {
    std::vector<std::string> result;
    result.reserve(unlocked_level_ids.size());
    for (const auto &level_id : unlocked_level_ids) {
        result.push_back("NEW UNLOCK: " + format_level_name(level_id) + "!");
    }

    return result;
}

std::string ProgressionPresentation::format_level_button_label(const ProgressionLevelUnlock &level_unlock, bool unlocked, bool completed, int best_score) {
    std::string label_text = format_level_name(level_unlock.level_id);
    if (!unlocked) {
        if (!level_unlock.requires_completed.empty()) {
            label_text += " (Complete " + format_level_name(level_unlock.requires_completed) + ")";
        } else {
            label_text += " (Locked)";
        }
        return label_text;
    }

    if (completed) {
        label_text += " - Best: " + std::to_string(best_score);
    }

    return label_text;
}

ProgressionUpgradeCardViewModel ProgressionPresentation::build_upgrade_card_view_model(const ProgressionUpgradePresentation &card, int owned_count) {
    return {
        .id = card.id,
        .name = card.name,
        .description = card.description,
        .emoji = card.emoji,
        .category = card.category,
        .owned_count = owned_count,
    };
}

ProgressionRewardViewModel ProgressionPresentation::build_reward_view_model(const ProgressionRewardDraft &draft,
                                                                            const std::vector<ProgressionUpgradePresentation> &upgrade_presentations) {
    ProgressionRewardViewModel result;
    result.source = to_progression_reward_source_id(draft.source);
    result.level_id = draft.level_id;
    result.title = format_reward_title(draft.source, draft.level_id);
    result.subtitle = format_reward_subtitle(draft.source, draft.level_id);

    result.available_upgrades.reserve(draft.upgrade_ids.size());
    for (const auto &upgrade_id : draft.upgrade_ids) {
        if (const ProgressionUpgradePresentation *presentation = find_presentation(upgrade_presentations, upgrade_id); presentation != nullptr) {
            result.available_upgrades.push_back(build_upgrade_card_view_model(*presentation));
        }
    }
    return result;
}

} // namespace defn