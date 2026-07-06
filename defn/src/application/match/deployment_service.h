#ifndef DEPLOYMENT_SERVICE_H
#define DEPLOYMENT_SERVICE_H

#include "match_outputs.h"
#include "match_session.h"
#include "progression_service.h"
#include "runtime_service_interfaces.h"
#include "unit_data.h"

#include <optional>
#include <string>

namespace defn {

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
    std::string unit_id;
    int unit_cost = 0;
    int remaining_energy = 0;
    std::optional<SpawnUnitIntent> spawn_intent;
};

class DeploymentService {
  public:
    void configure(MatchSession *match_session, const UnitCatalog *unit_catalog, const ProgressionService *progression, const GridQueryService *grid);
    DeploymentResult deploy_friendly(const std::string &unit_id);

  private:
    MatchSession *match_session_ = nullptr;
    const UnitCatalog *unit_catalog_ = nullptr;
    const ProgressionService *progression_ = nullptr;
    const GridQueryService *grid_ = nullptr;
};

} // namespace defn

#endif