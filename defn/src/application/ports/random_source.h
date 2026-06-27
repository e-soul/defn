#ifndef RANDOM_SOURCE_H
#define RANDOM_SOURCE_H

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
    StdRandomSource();

    int range_int(int inclusive_min, int inclusive_max) override;
    float range_real(float min, float max) override;

  private:
    std::mt19937 rng_;
};

} // namespace defn

#endif