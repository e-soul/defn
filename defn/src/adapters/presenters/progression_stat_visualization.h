#ifndef PROGRESSION_STAT_VISUALIZATION_H
#define PROGRESSION_STAT_VISUALIZATION_H

#include <array>
#include <string>

namespace defn {

constexpr std::size_t PROGRESSION_STAT_SEGMENT_COUNT = 5;

enum class ProgressionStatIcon {
    GENERIC,
    SHIELD,
    RETICLE,
    MOBILITY,
    BATTERY,
    FIRE_RATE,
    INTEGRITY,
    ENERGY,
    BOUNTY,
};

enum class ProgressionStatDirection {
    MORE_IS_STRONGER,
    MORE_IS_EXPENSIVE,
};

struct ProgressionStatPresentationConfig {
    double quantum = 1.0;
    ProgressionStatIcon icon = ProgressionStatIcon::GENERIC;
    ProgressionStatDirection direction = ProgressionStatDirection::MORE_IS_STRONGER;
};

struct ProgressionStatSegmentViewModel {
    int foundation_tier = -1;
    int promoted_tier = 0;
    double promotion_fraction = 0.0;
    bool upgrade_emphasis = false;
};

struct ProgressionStatVisualViewModel {
    std::string stat_id;
    ProgressionStatIcon icon = ProgressionStatIcon::GENERIC;
    ProgressionStatDirection direction = ProgressionStatDirection::MORE_IS_STRONGER;
    double quantum = 1.0;
    std::array<ProgressionStatSegmentViewModel, PROGRESSION_STAT_SEGMENT_COUNT> segments{};
    std::string exact_value;
    std::string exact_detail;
};

[[nodiscard]] ProgressionStatPresentationConfig progression_stat_config(const std::string &stat_id);
[[nodiscard]] std::array<ProgressionStatSegmentViewModel, PROGRESSION_STAT_SEGMENT_COUNT> normalize_progression_stat(double value, double quantum);
[[nodiscard]] ProgressionStatVisualViewModel make_progression_stat_visual(const std::string &stat_id, double base_value, double effective_value,
                                                                          const std::string &exact_value, bool contribution_only);

} // namespace defn

#endif
