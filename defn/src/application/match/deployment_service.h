#ifndef DEPLOYMENT_SERVICE_H
#define DEPLOYMENT_SERVICE_H

#include "match_session.h"
#include "progression_service.h"
#include "runtime_service_interfaces.h"
#include "unit_data.h"
#include "unit_spawn_request.h"

#include <godot_cpp/variant/string.hpp>
#include <optional>

namespace defn {

using namespace godot;

enum class DeploymentFailureReason {
    NONE,
    GAME_OVER,
    MISSING_DEPENDENCY,
    UNKNOWN_UNIT,
    INSUFFICIENT_ENERGY,
    MISSING_GRID,
};

struct DeploymentResult {
    bool succeeded = false;
    DeploymentFailureReason failure_reason = DeploymentFailureReason::NONE;
    String unit_type;
    int unit_cost = 0;
    int remaining_energy = 0;
    std::optional<UnitSpawnRequest> spawn_request;
};

class DeploymentService {
  public:
    void configure(MatchSession *match_session, const UnitCatalog *unit_catalog, const ProgressionService *progression, const GridQueryService *grid);
    DeploymentResult deploy_friendly(const String &unit_type);

  private:
    MatchSession *match_session_ = nullptr;
    const UnitCatalog *unit_catalog_ = nullptr;
    const ProgressionService *progression_ = nullptr;
    const GridQueryService *grid_ = nullptr;
};

} // namespace defn

#endif