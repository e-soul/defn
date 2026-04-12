#include "deployment_service.h"

#include "grid_manager.h"
#include "progression_manager.h"
#include "unit.h"
#include "unit_factory.h"

namespace defn {

namespace {

std::optional<UnitConfig> resolve_friendly_config(const CampaignService *progression, const UnitDataLoader *unit_data, const String &unit_type) {
    if (progression == nullptr || unit_data == nullptr) {
        return std::nullopt;
    }

    const auto base_config = unit_data->get_unit(unit_type);
    if (!base_config || base_config->side != UnitSide::FRIENDLY) {
        return std::nullopt;
    }

    return progression->get_effective_friendly_unit_config(*base_config);
}

} // namespace

void DeploymentService::configure(MatchSession *match_session, const UnitDataLoader *unit_data, CampaignService *progression, GridManager *grid) {
    match_session_ = match_session;
    unit_data_ = unit_data;
    progression_ = progression;
    grid_ = grid;
}

DeploymentResult DeploymentService::deploy_friendly(const String &unit_type) {
    DeploymentResult result;
    result.unit_type = unit_type;

    if (match_session_ == nullptr || unit_data_ == nullptr || progression_ == nullptr) {
        result.failure_reason = DeploymentFailureReason::MISSING_DEPENDENCY;
        return result;
    }

    if (match_session_->is_game_over()) {
        result.failure_reason = DeploymentFailureReason::GAME_OVER;
        result.remaining_energy = match_session_->get_core_resource();
        return result;
    }

    const auto config = resolve_friendly_config(progression_, unit_data_, unit_type);
    if (!config) {
        result.failure_reason = DeploymentFailureReason::UNKNOWN_UNIT;
        result.remaining_energy = match_session_->get_core_resource();
        return result;
    }

    result.unit_cost = config->cost;
    if (!match_session_->can_spend_energy(config->cost)) {
        result.failure_reason = DeploymentFailureReason::INSUFFICIENT_ENERGY;
        result.remaining_energy = match_session_->get_core_resource();
        return result;
    }

    if (grid_ == nullptr) {
        result.failure_reason = DeploymentFailureReason::MISSING_GRID;
        result.remaining_energy = match_session_->get_core_resource();
        return result;
    }

    match_session_->spend_energy(config->cost);
    const real_t spawn_x_pos = grid_->deploy_x();
    const real_t spawn_y_pos = GridManager::random_belt_y();

    result.succeeded = true;
    result.failure_reason = DeploymentFailureReason::NONE;
    result.remaining_energy = match_session_->get_core_resource();
    result.unit = UnitFactory::create(*config, Vector2(spawn_x_pos, spawn_y_pos));
    return result;
}

} // namespace defn