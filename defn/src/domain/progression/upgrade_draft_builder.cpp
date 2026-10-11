// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "upgrade_draft_builder.h"

#include <algorithm>
#include <utility>

namespace defn {

namespace {

int choose_weighted_index(const std::vector<const UpgradeDraftCard *> &candidates, RandomSource &random) {
    int total_weight = 0;
    for (const UpgradeDraftCard *candidate : candidates) {
        total_weight += std::max(candidate->weight, 1);
    }

    int roll = random.range_int(1, total_weight);
    for (int index = 0; std::cmp_less(index, candidates.size()); ++index) {
        roll -= std::max(candidates[static_cast<size_t>(index)]->weight, 1);
        if (roll <= 0) {
            return index;
        }
    }

    return static_cast<int>(candidates.size()) - 1;
}

const UpgradeDraftCard *pick_and_remove(std::vector<const UpgradeDraftCard *> &candidates, RandomSource &random) {
    if (candidates.empty()) {
        return nullptr;
    }

    const int index = choose_weighted_index(candidates, random);
    const UpgradeDraftCard *choice = candidates[static_cast<size_t>(index)];
    candidates.erase(candidates.begin() + index);
    return choice;
}

} // namespace

bool upgrade_draft_eligible(const ProgressionProfile &profile, const UpgradeDraftCard &card) {
    return get_owned_upgrade_count(profile, card.id) < card.max_picks && get_completed_level_count(profile) >= card.minimum_completed_levels &&
           std::ranges::all_of(card.prerequisites, [&](const std::string &prerequisite) { return has_upgrade(profile, prerequisite); });
}

std::vector<std::string> build_upgrade_draft(const ProgressionProfile &profile, const std::vector<UpgradeDraftCard> &cards, const UpgradeDraftConfig &config,
                                             RandomSource &random) {
    std::vector<const UpgradeDraftCard *> unit_candidates;
    std::vector<const UpgradeDraftCard *> general_candidates;
    for (const auto &card : cards) {
        if (!upgrade_draft_eligible(profile, card)) {
            continue;
        }

        if (card.grants_unit_unlock) {
            unit_candidates.push_back(&card);
        } else {
            general_candidates.push_back(&card);
        }
    }

    std::vector<const UpgradeDraftCard *> selected_cards;
    const int draft_size = std::max(1, config.draft_size);

    if (config.reserve_unit_slot) {
        if (const UpgradeDraftCard *unit_pick = pick_and_remove(unit_candidates, random); unit_pick != nullptr) {
            selected_cards.push_back(unit_pick);
        }
    }

    std::vector<const UpgradeDraftCard *> remaining_candidates = general_candidates;
    remaining_candidates.insert(remaining_candidates.end(), unit_candidates.begin(), unit_candidates.end());

    while (static_cast<int>(selected_cards.size()) < draft_size) {
        const UpgradeDraftCard *pick = pick_and_remove(remaining_candidates, random);
        if (pick == nullptr) {
            break;
        }
        selected_cards.push_back(pick);
    }

    std::vector<std::string> result;
    result.reserve(selected_cards.size());
    for (const UpgradeDraftCard *card : selected_cards) {
        result.push_back(card->id);
    }
    return result;
}

} // namespace defn
