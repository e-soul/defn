#include "test_harness.h"

#include "random_source.h"

namespace defn {

DEFN_TEST(std_random_source_returns_values_within_integer_bounds) {
    StdRandomSource random;

    for (int sample = 0; sample < 64; ++sample) {
        const int value = random.range_int(2, 5);
        DEFN_CHECK(value >= 2);
        DEFN_CHECK(value <= 5);
    }
}

DEFN_TEST(std_random_source_accepts_reversed_integer_bounds) {
    StdRandomSource random;

    for (int sample = 0; sample < 64; ++sample) {
        const int value = random.range_int(5, 2);
        DEFN_CHECK(value >= 2);
        DEFN_CHECK(value <= 5);
    }
}

DEFN_TEST(std_random_source_returns_values_within_real_bounds) {
    StdRandomSource random;

    for (int sample = 0; sample < 64; ++sample) {
        const float value = random.range_real(1.5F, 2.5F);
        DEFN_CHECK(value >= 1.5F);
        DEFN_CHECK(value <= 2.5F);
    }
}

DEFN_TEST(std_random_source_accepts_reversed_real_bounds) {
    StdRandomSource random;

    for (int sample = 0; sample < 64; ++sample) {
        const float value = random.range_real(2.5F, 1.5F);
        DEFN_CHECK(value >= 1.5F);
        DEFN_CHECK(value <= 2.5F);
    }
}

DEFN_TEST(std_random_source_returns_degenerate_bounds) {
    StdRandomSource random;

    DEFN_CHECK_EQ(random.range_int(7, 7), 7);
    DEFN_CHECK_CLOSE(random.range_real(3.25F, 3.25F), 3.25F, 0.0001);
}

} // namespace defn