// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef RANDOM_SOURCE_H
#define RANDOM_SOURCE_H

#include <cstdint>
#include <random>

namespace defn {

class RandomSource {
  public:
    virtual ~RandomSource() = default;

    virtual int range_int(int inclusive_min, int inclusive_max) = 0;
    virtual float range_real(float min, float max) = 0;
};

class StdRandomSource final : public RandomSource {
  public:
    // Unseeded: the shipped game wants a different sequence every launch.
    StdRandomSource();
    // Seeded: reproducible sequences for tests and for simulation runs that must replay identically.
    explicit StdRandomSource(std::uint32_t seed);

    int range_int(int inclusive_min, int inclusive_max) override;
    float range_real(float min, float max) override;

  private:
    std::mt19937 rng_;
};

} // namespace defn

#endif