#include "deployment_service.h"

#include "unit_runtime_config_resolver.h"

namespace defn {

namespace {

ProgressionUnitStats to_progression_unit_stats(const UnitConfig &config) {
    return {
        .unit_id = config.name,
        .friendly = config.side == UnitSide::FRIENDLY,
        .hp = config.hp,
        .ranged_damage = config.ranged_damage,
        .move_speed = static_cast<float>(config.move_speed_pixels_per_second),
        .has_projectile_attack = config.projectile_attack.has_value(),
    };
}

void apply_progression_unit_stats(UnitConfig &config, const ProgressionUnitStats &stats) {
    config.hp = stats.hp;
    config.ranged_damage = stats.ranged_damage;
    config.move_speed_pixels_per_second = stats.move_speed;
}

std::optional<UnitConfig> resolve_friendly_config(const ProgressionService *progression, const UnitCatalog *unit_catalog, const std::string &unit_id) {
    if (progression == nullptr || unit_catalog == nullptr) {
        return std::nullopt;
    }

    if (unit_id == "base") {
        return std::nullopt;
    }

    const auto base_config = unit_catalog->get_unit(unit_id);
    if (!base_config || base_config->side != UnitSide::FRIENDLY) {
        return std::nullopt;
    }

    UnitConfig effective_config = *base_config;
    apply_progression_unit_stats(effective_config, progression->get_effective_friendly_unit_stats(to_progression_unit_stats(*base_config)));
    return effective_config;
}

} // namespace

void DeploymentService::configure(MatchSession *match_session, const UnitCatalog *unit_catalog, const ProgressionService *progression,
                                  const GridQueryService *grid, RandomSource *random) {
    match_session_ = match_session;
    unit_catalog_ = unit_catalog;
    progression_ = progression;
    grid_ = grid;
    random_ = random;
}

DeploymentResult DeploymentService::deploy_friendly(const std::string &unit_id) {
    DeploymentResult result;
    result.unit_id = unit_id;

    if (match_session_ == nullptr || unit_catalog_ == nullptr || progression_ == nullptr || random_ == nullptr) {
        result.failure_reason = DeploymentFailureReason::MISSING_DEPENDENCY;
        return result;
    }

    if (match_session_->is_game_over()) {
        result.failure_reason = DeploymentFailureReason::GAME_OVER;
        result.remaining_energy = match_session_->get_core_resource();
        return result;
    }

    const auto config = resolve_friendly_config(progression_, unit_catalog_, unit_id);
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
    const double spawn_x_pos = grid_->deploy_x();
    const double spawn_y_pos = grid_->sample_belt_y();

    result.succeeded = true;
    result.failure_reason = DeploymentFailureReason::NONE;
    result.remaining_energy = match_session_->get_core_resource();
    result.spawn_intent = SpawnUnitIntent{
        .unit_id = unit_id,
        .side = MatchUnitSide::Friendly,
        .position = {.x = spawn_x_pos, .y = spawn_y_pos},
        .runtime_profile = UnitRuntimeProfile::from_unit_config(*config),
        .resolved_runtime_config = resolve_unit_runtime_config(to_runtime_range_config(*config), *random_),
    };
    return result;
}

} // namespace defn