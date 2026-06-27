#ifndef SCRIPTED_RANDOM_SOURCE_H
#define SCRIPTED_RANDOM_SOURCE_H

#include "random_source.h"

#include <queue>

namespace defn::tests {

class ScriptedRandomSource final : public RandomSource {
  public:
    void push_int(int value) { ints_.push(value); }
    void push_real(float value) { reals_.push(value); }

    int range_int(int /*inclusive_min*/, int /*inclusive_max*/) override {
        const int value = ints_.front();
        ints_.pop();
        return value;
    }

    float range_real(float /*min*/, float /*max*/) override {
        const float value = reals_.front();
        reals_.pop();
        return value;
    }

  private:
    std::queue<int> ints_;
    std::queue<float> reals_;
};

} // namespace defn::tests

#endif