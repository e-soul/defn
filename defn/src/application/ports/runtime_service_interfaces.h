#ifndef RUNTIME_SERVICE_INTERFACES_H
#define RUNTIME_SERVICE_INTERFACES_H

namespace defn {

class GridQueryService {
  public:
    virtual ~GridQueryService() = default;

    [[nodiscard]] virtual double deploy_x() const = 0;
    [[nodiscard]] virtual double spawn_x() const = 0;
    [[nodiscard]] virtual double sample_belt_y() const = 0;
};

} // namespace defn

#endif