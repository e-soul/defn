// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause
#include "score_pagination.h"
#include "test_harness.h"
namespace defn {
DEFN_TEST(score_pages_reserve_chrome_before_measured_capacity_and_keep_item_anchors) {
    const std::vector<MeasuredScoreLine> lines{{0, 2, 30}, {2, 2, 40}, {4, 1, 20}};
    DEFN_CHECK(paginate_score_lines(lines, 75, 5) == std::vector<std::size_t>({0, 4}));
    DEFN_CHECK(paginate_score_lines(lines, 74, 5) == std::vector<std::size_t>({0, 2}));
    DEFN_CHECK(paginate_score_lines(lines, 10, 5) == std::vector<std::size_t>({0, 2, 4}));
    DEFN_CHECK_EQ(score_grid_capacity(125, 60, 5, 3, 2), 6U);
    DEFN_CHECK_EQ(score_grid_capacity(124, 60, 5, 3, 2), 3U);
    DEFN_CHECK_EQ(score_grid_capacity(0, 60, 5, 0, 0), 1U);
    auto starts = paginate_score_grid(17, 6);
    DEFN_CHECK_EQ(starts.size(), 3U);
    DEFN_CHECK_EQ(score_page_for_anchor(starts, 13), 2U);
    starts = paginate_score_grid(17, 2);
    DEFN_CHECK_EQ(score_page_for_anchor(starts, 13), 6U);
    DEFN_CHECK_EQ(score_page_for_anchor(starts, 99), 8U);
    DEFN_CHECK_EQ(paginate_score_grid(0, 0).size(), 1U);
}
} // namespace defn
