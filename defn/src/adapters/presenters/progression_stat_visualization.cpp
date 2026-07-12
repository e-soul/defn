#include "progression_stat_visualization.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace defn {

namespace {

constexpr double EPSILON = 1.0e-9;

using SegmentArray = std::array<ProgressionStatSegmentViewModel, PROGRESSION_STAT_SEGMENT_COUNT>;

bool segments_differ(const ProgressionStatSegmentViewModel &base, const ProgressionStatSegmentViewModel &effective) {
    return base.foundation_tier != effective.foundation_tier || base.promoted_tier != effective.promoted_tier ||
           std::fabs(base.promotion_fraction - effective.promotion_fraction) > EPSILON;
}

} // namespace

ProgressionStatPresentationConfig progression_stat_config(const std::string &stat_id) {
    static const std::unordered_map<std::string, ProgressionStatPresentationConfig> configs = {
        {"health", {.quantum = 100.0, .icon = ProgressionStatIcon::SHIELD}},
        {"firepower", {.quantum = 4.0, .icon = ProgressionStatIcon::RETICLE}},
        {"mobility", {.quantum = 20.0, .icon = ProgressionStatIcon::MOBILITY}},
        {"deploy_cost", {.quantum = 5.0, .icon = ProgressionStatIcon::BATTERY, .direction = ProgressionStatDirection::MORE_IS_EXPENSIVE}},
        {"fire_rate", {.quantum = 0.25, .icon = ProgressionStatIcon::FIRE_RATE}},
        {"integrity_bonus", {.quantum = 1.0, .icon = ProgressionStatIcon::INTEGRITY}},
        {"starting_energy_bonus", {.quantum = 10.0, .icon = ProgressionStatIcon::ENERGY}},
        {"energy_generation", {.quantum = 0.5, .icon = ProgressionStatIcon::ENERGY}},
        {"bounty_bonus", {.quantum = 10.0, .icon = ProgressionStatIcon::BOUNTY}},
    };
    if (const auto found = configs.find(stat_id); found != configs.end()) {
        return found->second;
    }
    return {};
}

SegmentArray normalize_progression_stat(double value, double quantum) {
    SegmentArray result{};
    if (!std::isfinite(quantum) || quantum <= 0.0) {
        quantum = 1.0;
    }
    const double normalized_value = std::isfinite(value) ? std::max(0.0, value) : 0.0;
    const double tier_one_capacity = static_cast<double>(PROGRESSION_STAT_SEGMENT_COUNT) * quantum;

    int promoted_tier = 0;
    double foundation_capacity = 0.0;
    double promotion_step = quantum;
    if (normalized_value > tier_one_capacity + EPSILON) {
        promoted_tier = 1;
        double upper_capacity = tier_one_capacity * 2.0;
        while (normalized_value > upper_capacity + EPSILON && upper_capacity < std::numeric_limits<double>::max() / 2.0) {
            ++promoted_tier;
            upper_capacity *= 2.0;
        }
        foundation_capacity = upper_capacity / 2.0;
        promotion_step = quantum * std::ldexp(1.0, promoted_tier - 1);
    }

    double promoted_slots = (normalized_value - foundation_capacity) / promotion_step;
    promoted_slots = std::clamp(promoted_slots, 0.0, static_cast<double>(PROGRESSION_STAT_SEGMENT_COUNT));
    const double nearest_integer = std::round(promoted_slots);
    if (std::fabs(promoted_slots - nearest_integer) <= EPSILON) {
        promoted_slots = nearest_integer;
    }

    for (std::size_t index = 0; index < result.size(); ++index) {
        auto &segment = result[index];
        segment.foundation_tier = promoted_tier == 0 ? -1 : promoted_tier - 1;
        segment.promoted_tier = promoted_tier;
        segment.promotion_fraction = std::clamp(promoted_slots - static_cast<double>(index), 0.0, 1.0);
    }
    return result;
}

ProgressionStatVisualViewModel make_progression_stat_visual(const std::string &stat_id, double base_value, double effective_value,
                                                            const std::string &exact_value, bool contribution_only) {
    const ProgressionStatPresentationConfig config = progression_stat_config(stat_id);
    const double display_base = contribution_only ? 0.0 : base_value;
    const double scale = stat_id == "bounty_bonus" ? 100.0 : 1.0;
    const SegmentArray base_segments = normalize_progression_stat(display_base * scale, config.quantum);
    SegmentArray effective_segments = normalize_progression_stat(effective_value * scale, config.quantum);
    for (std::size_t index = 0; index < effective_segments.size(); ++index) {
        effective_segments[index].upgrade_emphasis = segments_differ(base_segments[index], effective_segments[index]);
    }

    ProgressionStatVisualViewModel result{.stat_id = stat_id,
                                          .icon = config.icon,
                                          .direction = config.direction,
                                          .quantum = config.quantum,
                                          .segments = effective_segments,
                                          .exact_value = exact_value,
                                          .exact_detail = exact_value};
    return result;
}

} // namespace defn
