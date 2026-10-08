// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#ifndef SCORE_PAGINATION_H
#define SCORE_PAGINATION_H
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>
namespace defn {
struct MeasuredScoreLine {
    std::size_t first;
    std::size_t count;
    float height;
};
inline std::vector<std::size_t> paginate_score_lines(std::span<const MeasuredScoreLine> lines, float available, float gap) {
    std::vector<std::size_t> starts{0};
    float used = 0;
    for (const auto &line : lines) {
        if (used > 0 && used + gap + line.height > available) {
            starts.push_back(line.first);
            used = 0;
        }
        used += line.height + (used > 0 ? gap : 0);
    }
    return starts;
}
inline std::size_t score_grid_capacity(float available, float card_height, float gap, std::size_t columns, std::size_t max_rows) {
    const auto rows = static_cast<std::size_t>(std::max(1.0F, std::floor((available + gap) / std::max(1.0F, card_height + gap))));
    return std::min(rows, std::max<std::size_t>(1, max_rows)) * std::max<std::size_t>(1, columns);
}
inline std::vector<std::size_t> paginate_score_grid(std::size_t count, std::size_t capacity) {
    std::vector<std::size_t> starts{0};
    capacity = std::max<std::size_t>(1, capacity);
    for (std::size_t i = capacity; i < count; i += capacity) {
        starts.push_back(i);
    }
    return starts;
}
inline std::size_t score_page_for_anchor(std::span<const std::size_t> starts, std::size_t anchor) {
    if (starts.empty()) {
        return 0;
    }
    const auto end = std::ranges::upper_bound(starts, anchor);
    return end == starts.begin() ? 0 : static_cast<std::size_t>(end - starts.begin() - 1);
}
} // namespace defn
#endif
