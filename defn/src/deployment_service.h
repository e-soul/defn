#ifndef DEPLOYMENT_SERVICE_H
#define DEPLOYMENT_SERVICE_H

#include "match_session.h"
#include "unit_data.h"

#include <godot_cpp/variant/string.hpp>

namespace defn {

using namespace godot;

class GridManager;
class CampaignService;
class Unit;

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
    Unit *unit = nullptr;
};

class DeploymentService {
  public:
    void configure(MatchSession *match_session, const UnitDataLoader *unit_data, CampaignService *progression, GridManager *grid);
    DeploymentResult deploy_friendly(const String &unit_type);

  private:
    MatchSession *match_session_ = nullptr;
    const UnitDataLoader *unit_data_ = nullptr;
    CampaignService *progression_ = nullptr;
    GridManager *grid_ = nullptr;
};

} // namespace defn

#endif