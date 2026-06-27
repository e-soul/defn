#include "random_source.h"

#include <algorithm>

namespace defn {

StdRandomSource::StdRandomSource() : rng_(std::random_device{}()) {}

int StdRandomSource::range_int(int inclusive_min, int inclusive_max) {
    if (inclusive_min > inclusive_max) {
        std::swap(inclusive_min, inclusive_max);
    }

    std::uniform_int_distribution<int> distribution(inclusive_min, inclusive_max);
    return distribution(rng_);
}

float StdRandomSource::range_real(float min, float max) {
    if (min > max) {
        std::swap(min, max);
    }

    std::uniform_real_distribution<float> distribution(min, max);
    return distribution(rng_);
}

} // namespace defn