#include "deployment_service.h"

namespace defn {

namespace {

std::optional<UnitConfig> resolve_friendly_config(const ProgressionService *progression, const UnitCatalog *unit_catalog, const String &unit_type) {
    if (progression == nullptr || unit_catalog == nullptr) {
        return std::nullopt;
    }

    if (unit_type == "base") {
        return std::nullopt;
    }

    const auto base_config = unit_catalog->get_unit(unit_type);
    if (!base_config || base_config->side != UnitSide::FRIENDLY) {
        return std::nullopt;
    }

    return progression->get_effective_friendly_unit_config(*base_config);
}

} // namespace

void DeploymentService::configure(MatchSession *match_session, const UnitCatalog *unit_catalog, const ProgressionService *progression,
                                  const GridQueryService *grid) {
    match_session_ = match_session;
    unit_catalog_ = unit_catalog;
    progression_ = progression;
    grid_ = grid;
}

DeploymentResult DeploymentService::deploy_friendly(const String &unit_type) {
    DeploymentResult result;
    result.unit_type = unit_type;

    if (match_session_ == nullptr || unit_catalog_ == nullptr || progression_ == nullptr) {
        result.failure_reason = DeploymentFailureReason::MISSING_DEPENDENCY;
        return result;
    }

    if (match_session_->is_game_over()) {
        result.failure_reason = DeploymentFailureReason::GAME_OVER;
        result.remaining_energy = match_session_->get_core_resource();
        return result;
    }

    const auto config = resolve_friendly_config(progression_, unit_catalog_, unit_type);
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
    const real_t spawn_y_pos = grid_->sample_belt_y();

    result.succeeded = true;
    result.failure_reason = DeploymentFailureReason::NONE;
    result.remaining_energy = match_session_->get_core_resource();
    result.spawn_request = UnitSpawnRequest{
        .config = *config,
        .position = Vector2(spawn_x_pos, spawn_y_pos),
    };
    return result;
}

} // namespace defn