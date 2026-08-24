// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "sim_match.h"

#include <algorithm>
#include <utility>

namespace defn {

namespace {

constexpr double ENERGY_TICK_SECONDS = 1.0; // GameManager's core resource Timer
constexpr double FRONT_LINE_SAMPLE_SECONDS = 1.0;
constexpr double SPIKE_WINDOW_SECONDS = 5.0;

UnitSide to_unit_side(MatchUnitSide side) { return side == MatchUnitSide::Friendly ? UnitSide::FRIENDLY : UnitSide::HOSTILE; }

// GridManager::configure resolves the level's belt ratios to screen coordinates at match start.
GameplayRules make_belt_rules(const GameplayRules &rules, const LevelDefinition &level) {
    GameplayRules adjusted = rules;
    adjusted.belt_top_y = std::min(level.belt_width_ratio.x, level.belt_width_ratio.y) * rules.viewport_height;
    adjusted.belt_bottom_y = std::max(level.belt_width_ratio.x, level.belt_width_ratio.y) * rules.viewport_height;
    return adjusted;
}

// The most enemies that appeared inside any five-second window, which is the spike density BALANCE.md tunes against.
int peak_spawn_window(const std::vector<double> &spawn_times) {
    int peak = 0;
    for (std::size_t start = 0; start < spawn_times.size(); ++start) {
        int count = 0;
        for (std::size_t index = start; index < spawn_times.size(); ++index) {
            if (spawn_times[index] - spawn_times[start] > SPIKE_WINDOW_SECONDS) {
                break;
            }
            ++count;
        }
        peak = std::max(peak, count);
    }

    return peak;
}

} // namespace

SimMatch::SimMatch(const UnitCatalog &catalog, const GlobalUnitConfig &globals, const LevelDefinition &level, const SimScenario &scenario,
                   const std::vector<std::string> &base_unit_ids, const std::vector<ProgressionUpgradeCard> &upgrade_cards)
    : scenario_(scenario), level_(level), random_(scenario.seed), grid_(make_belt_rules(globals.gameplay_rules, level), random_),
      world_(catalog, globals, random_) {
    const GameplayRules &rules = grid_.get_rules();
    const float world_width = scenario_.world_width.value_or(rules.viewport_width * static_cast<float>(rules.world_multiplier));
    grid_.set_world_width(world_width);
    camera_.configure(rules, world_width, scenario_.camera);
    grid_.set_camera_x(camera_.get_position().x);

    progression_.configure(base_unit_ids, upgrade_cards, scenario_.owned_upgrades);
    policy_ = make_policy(scenario_.policy);

    director_.configure(&progression_, &catalog, &grid_, &random_);
    director_.load_level_definition(level_, scenario_.level_id);
}

void SimMatch::begin() {
    director_.begin_match();
    roster_ = director_.build_available_friendlies();

    // The base is a stationary entity carrying the "base" unit entry, with the health the match configuration decided.
    const Vector2 base_ratio = director_.get_base_position_ratio();
    const GameplayRules &rules = grid_.get_rules();
    const SimSpawnResult base = world_.spawn("base", UnitSide::FRIENDLY, {.x = base_ratio.x * rules.viewport_width, .y = base_ratio.y * rules.viewport_height},
                                             {.hp = director_.get_base_max_health()});
    base_id_ = base.id;

    // Everything placed before the match starts is already in the process group when the first frame runs.
    world_.begin_run();
}

void SimMatch::tick() {
    const double delta = world_.get_fixed_delta_seconds();

    // GameManager::_process bails out once the match is over, and so does everything under it.
    if (director_.is_game_over()) {
        finished_ = true;
        return;
    }

    // 1. Waves, spawn intents and the victory check.
    apply_match_update(director_.update(delta));

    // 2. The energy timer, which GameManager runs on a one-second Timer rather than per frame.
    energy_tick_accumulator_ += delta;
    while (energy_tick_accumulator_ >= ENERGY_TICK_SECONDS) {
        energy_tick_accumulator_ -= ENERGY_TICK_SECONDS;
        apply_match_update(director_.handle_core_resource_tick());
    }

    // 3. What the player does.
    run_policy();

    // 4 and 5. Entities fight, then projectiles fly.
    world_.tick();

    // 6. Deaths become bounty, base damage and leak events.
    report_deaths();

    // 7. The camera, which decides where the next spawn lands.
    camera_.update(delta, grid_, world_.get_mutable_entities());

    elapsed_seconds_ += delta;
    sample_metrics();

    if (director_.is_game_over()) {
        finished_ = true;
    }
}

void SimMatch::apply_match_update(const MatchUpdate &update) {
    for (const SpawnUnitIntent &intent : update.spawn_unit_intents) {
        const SimSpawnResult spawned =
            world_.spawn(intent.unit_id, to_unit_side(intent.side), {.x = static_cast<float>(intent.position.x), .y = static_cast<float>(intent.position.y)});
        if (!spawned.succeeded()) {
            continue;
        }

        ++unit_tallies_[intent.unit_id].spawned;
        if (intent.side == MatchUnitSide::Hostile) {
            hostile_spawn_times_.push_back(elapsed_seconds_);
        }
    }

    if (update.wave_changed.has_value()) {
        current_wave_ = update.wave_changed->current_wave;
    }

    if (update.match_ended.has_value()) {
        finished_ = true;
        victory_ = update.match_ended->victory;
        ending_summary_ = update.match_ended->summary_model;
        decided_at_seconds_ = elapsed_seconds_;
    }
}

void SimMatch::run_policy() {
    if (policy_ == nullptr) {
        return;
    }

    for (const PlayerCommand &command : policy_->decide(observe())) {
        if (command.kind != PlayerCommand::Kind::DEPLOY) {
            continue;
        }

        const int energy_before = director_.get_core_resource();
        const MatchUpdate update = director_.handle_deploy_request(command.unit_id);
        if (update.spawn_unit_intents.empty()) {
            continue; // unaffordable, unknown, or the match is over: the request simply does nothing
        }

        const int spent = energy_before - director_.get_core_resource();
        energy_spent_ += spent;
        ++deployment_counts_[command.unit_id];
        deployment_energy_[command.unit_id] += spent;
        apply_match_update(update);
    }
}

void SimMatch::report_deaths() {
    for (const SimDamageEvent &event : world_.drain_damage_events()) {
        if (event.target_id == base_id_) {
            const SimEntity *attacker = world_.find_entity(event.source_id);
            leak_events_.push_back({
                .time_seconds = event.time_seconds,
                .unit_id = attacker != nullptr ? attacker->unit_id : std::string(),
                .damage = event.damage,
            });
        }
    }

    // Deaths are collected before any of them is reported: reporting can spawn, and spawning moves the entity array.
    std::vector<DeathRecord> deaths;
    {
        const std::vector<SimEntity> &entities = world_.get_entities();
        death_reported_.resize(entities.size(), false);
        for (std::size_t index = 0; index < entities.size(); ++index) {
            const SimEntity &entity = entities[index];
            if (!entity.dead || death_reported_[index]) {
                continue;
            }
            death_reported_[index] = true;

            UnitTally &tally = unit_tallies_[entity.unit_id];
            ++tally.deaths;
            tally.total_lifespan_seconds += entity.death_time_seconds - entity.spawn_time_seconds;
            deaths.push_back({.id = entity.id, .side = entity.side, .bounty = entity.bounty});
        }
    }

    for (const DeathRecord &death : deaths) {
        if (death.id == base_id_) {
            apply_match_update(director_.handle_base_destroyed());
            continue;
        }
        if (death.side == UnitSide::HOSTILE) {
            apply_match_update(director_.handle_enemy_defeated({.bounty = death.bounty}));
        }
    }

    // The base reports its durability whenever it changes; polling once a tick reaches the same state.
    if (const SimEntity *base = find_base(); base != nullptr && !base->dead && base->hp != director_.get_base_health()) {
        apply_match_update(director_.handle_base_durability_changed(base->hp));
    }
}

void SimMatch::sample_metrics() {
    const double delta = world_.get_fixed_delta_seconds();
    energy_idle_integral_ += static_cast<double>(director_.get_core_resource()) * delta;

    int live_hostiles = 0;
    float front_line = 0.0F;
    bool has_front_line = false;
    for (const SimEntity &entity : world_.get_entities()) {
        if (entity.dead || entity.side != UnitSide::HOSTILE) {
            continue;
        }
        ++live_hostiles;
        if (!has_front_line || entity.position.x < front_line) {
            front_line = entity.position.x;
            has_front_line = true;
        }
    }
    peak_concurrent_enemies_ = std::max(peak_concurrent_enemies_, live_hostiles);

    front_line_sample_accumulator_ += delta;
    if (front_line_sample_accumulator_ >= FRONT_LINE_SAMPLE_SECONDS) {
        front_line_sample_accumulator_ -= FRONT_LINE_SAMPLE_SECONDS;
        front_line_trace_.push_back(has_front_line ? front_line : 0.0F);
    }
}

MatchObservation SimMatch::observe() const {
    const SimEntity *base = find_base();
    const float base_x = director_.get_base_position_ratio().x * grid_.get_rules().viewport_width;

    return {
        .elapsed_seconds = elapsed_seconds_,
        .energy = director_.get_core_resource(),
        .base_health = director_.get_base_health(),
        .base_max_health = director_.get_base_max_health(),
        .current_wave = current_wave_,
        .total_waves = director_.get_total_waves(),
        .base_position_x = base_x,
        .deploy_x = static_cast<float>(grid_.deploy_x()),
        .base_engage_x = base_x + (base != nullptr ? std::max(base->combat.ranged_range, base->combat.attack_range) : 0.0F),
        .entities = world_.get_entities(),
        .roster = roster_,
    };
}

const SimEntity *SimMatch::find_base() const { return world_.find_entity(base_id_); }

SimMatchReport SimMatch::build_report() const {
    SimMatchReport report;
    report.level_id = scenario_.level_id;
    report.seed = scenario_.seed;
    report.policy = policy_ != nullptr ? policy_->name() : "none";
    report.decided = director_.is_game_over();
    report.victory = victory_;
    report.clear_time_seconds = report.decided ? decided_at_seconds_ : elapsed_seconds_;

    const SimEntity *base = find_base();
    report.base_health = base != nullptr ? std::max(base->hp, 0) : 0;
    report.base_max_health = base != nullptr ? base->max_hp : 0;
    // MatchSession counts hearts the same way: any remainder still shows as a heart.
    report.remaining_integrity = (report.base_health + MatchSession::BASE_HEALTH_PER_HEART - 1) / MatchSession::BASE_HEALTH_PER_HEART;
    if (ending_summary_.has_value()) {
        report.remaining_integrity = ending_summary_->hearts_remaining;
        report.kill_score = ending_summary_->kill_score;
        report.level_score = ending_summary_->level_score;
    }

    report.energy_idle_integral = energy_idle_integral_;
    report.peak_concurrent_enemies = peak_concurrent_enemies_;
    report.peak_window_5s = peak_spawn_window(hostile_spawn_times_);
    report.energy_spent = energy_spent_;
    report.front_line_trace = front_line_trace_;
    report.leak_events = leak_events_;
    report.camera_scroll_events = camera_.get_scroll_events();

    for (const auto &[unit_id, count] : deployment_counts_) {
        report.deployments_total += count;
        report.deployments.push_back({.unit_id = unit_id, .count = count, .total_energy = deployment_energy_.at(unit_id)});
    }

    std::map<std::string, UnitTally> tallies = unit_tallies_;
    for (const SimEntity &entity : world_.get_entities()) {
        UnitTally &tally = tallies[entity.unit_id];
        tally.damage_dealt += entity.damage_dealt;
        tally.damage_taken += entity.damage_taken;
        tally.kills += entity.kills;
    }

    for (const auto &[unit_id, tally] : tallies) {
        report.per_unit.push_back({
            .unit_id = unit_id,
            .spawned = tally.spawned,
            .damage_dealt = tally.damage_dealt,
            .damage_taken = tally.damage_taken,
            .kills = tally.kills,
            .deaths = tally.deaths,
            .mean_lifespan_seconds = tally.deaths > 0 ? tally.total_lifespan_seconds / tally.deaths : 0.0,
        });
    }

    return report;
}

SimMatchReport SimMatch::run() {
    begin();
    while (!finished_ && elapsed_seconds_ < scenario_.max_seconds) {
        tick();
    }

    return build_report();
}

} // namespace defn
