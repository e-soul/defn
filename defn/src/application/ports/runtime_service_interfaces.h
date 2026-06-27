#ifndef RUNTIME_SERVICE_INTERFACES_H
#define RUNTIME_SERVICE_INTERFACES_H

#include <godot_cpp/core/math.hpp>

namespace defn {

using namespace godot;

class GridQueryService {
  public:
    virtual ~GridQueryService() = default;

    [[nodiscard]] virtual real_t deploy_x() const = 0;
    [[nodiscard]] virtual real_t spawn_x() const = 0;
    [[nodiscard]] virtual real_t sample_belt_y() const = 0;
};

} // namespace defn

#endif