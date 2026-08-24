// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SIM_WORLD_H
#define SIM_WORLD_H

#include "combat_use_cases.h"
#include "projectile_rules.h"
#include "random_source.h"
#include "sim_entity.h"
#include "sim_projectile.h"
#include "unit_definition.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace defn {

struct SimWorldConfig {
    double fixed_delta_seconds = 1.0 / 60.0;
};

enum class SimSpawnRejection { NONE, UNKNOWN_UNIT };

// Overrides for a spawn the level data does not describe on its own -- today only the base, whose health comes from
// the match configuration rather than from its unit entry.
struct SimSpawnOverrides {
    std::optional<int> hp;
};

// Every point of damage that landed, in the order it landed. The match driver turns these into bounty, base integrity
// and leak events; the conformance harness will read the same log.
struct SimDamageEvent {
    double time_seconds = 0.0;
    EntityId source_id;
    EntityId target_id;
    int damage = 0;
    bool lethal = false;
};

struct SimSpawnResult {
    EntityId id;
    SimSpawnRejection rejection = SimSpawnRejection::NONE;

    [[nodiscard]] bool succeeded() const { return rejection == SimSpawnRejection::NONE && id.is_valid(); }
};

struct SimSideSummary {
    int alive = 0;
    int hp_remaining = 0;
    int damage_dealt = 0;
};

struct SimEngagementReport {
    // True once at least one side has been wiped out. False means the run hit its time limit undecided.
    bool resolved = false;
    // Empty when the run was undecided, or when both sides were wiped out in the same tick.
    std::optional<UnitSide> winner;
    double duration_seconds = 0.0;
    SimSideSummary friendly;
    SimSideSummary hostile;
};

// A deterministic, engine-free belt. Entities stand on a strip, walk forward, pick targets and trade blows through the
// same domain rules the shipped game runs; nothing here knows about Godot, waves, economy or the camera.
//
// Every rule that decides an outcome is called, not reimplemented: `advance_combat` drives the state machine,
// `select_target_from_snapshots` picks targets, `UnitAnimationState` times the swings, `advance_projectile` flies the
// shots, `resolve_projectile_impact` resolves what they hit, and `FieldPromotionRuntime` grants promotions. The kernel
// only supplies the scene facts those rules would otherwise read off nodes.
class SimWorld {
  public:
    SimWorld(const UnitCatalog &catalog, const GlobalUnitConfig &globals, RandomSource &random, const SimWorldConfig &config = {});

    SimSpawnResult spawn(const std::string &unit_id, UnitSide side, Vector2 position, const SimSpawnOverrides &overrides = {});

    // Marks everything spawned so far as already present when the run begins, so the first tick steps it. Whatever
    // appears once the run is under way still waits a tick, exactly as a node added mid-frame does.
    void begin_run() { tick_index_ = std::max<std::uint64_t>(tick_index_, 1); }

    void tick();

    [[nodiscard]] double get_elapsed_seconds() const { return elapsed_seconds_; }
    [[nodiscard]] std::uint64_t get_tick_index() const { return tick_index_; }
    [[nodiscard]] double get_fixed_delta_seconds() const { return config_.fixed_delta_seconds; }
    [[nodiscard]] const std::vector<SimEntity> &get_entities() const { return entities_; }
    [[nodiscard]] const std::vector<SimProjectile> &get_projectiles() const { return projectiles_; }
    [[nodiscard]] std::span<SimEntity> get_mutable_entities() { return entities_; }
    // Damage that landed since the last call. Cleared by reading it.
    [[nodiscard]] std::vector<SimDamageEvent> drain_damage_events();
    [[nodiscard]] const SimEntity *find_entity(EntityId entity_id) const;
    [[nodiscard]] SimSideSummary summarize(UnitSide side) const;
    [[nodiscard]] int count_alive(UnitSide side) const;

  private:
    void step_entity(SimEntity &entity);
    void step_projectiles(std::size_t count_at_tick_start);
    void launch_pending_projectile(SimEntity &shooter);
    void detonate(SimProjectile &projectile);
    void build_snapshots(const SimEntity &viewer);
    void build_impact_snapshots(EntityId direct_target_id);
    void apply_commands(SimEntity &entity, const std::vector<CombatCommand> &commands);
    static void apply_pose(SimEntity &entity, CombatPoseIntent pose);
    void move(SimEntity &entity) const;
    void apply_damage(SimEntity &source, EntityId target_id, int base_damage);
    static void record_effective_damage_dealt(SimEntity &source, int effective_damage);
    [[nodiscard]] bool is_target_out_of_range(const SimEntity &viewer) const;
    SimEntity *find_mutable_entity(EntityId entity_id);

    const UnitCatalog &catalog_;
    GlobalUnitConfig globals_;
    RandomSource &random_;
    SimWorldConfig config_;

    float world_width_ = 0.0F;
    float friendly_world_margin_ = 0.0F;

    std::vector<SimEntity> entities_;
    std::vector<SimProjectile> projectiles_;
    std::vector<CombatTargetSnapshot> snapshots_;
    std::vector<SimDamageEvent> damage_events_;
    std::vector<ProjectileTargetSnapshot> impact_snapshots_;
    uint64_t next_entity_id_ = 1;
    uint64_t next_projectile_id_ = 1;
    std::uint64_t tick_index_ = 0;
    double elapsed_seconds_ = 0.0;
};

// Ticks the world until one side is wiped out or the time limit is reached. The lab question -- how does N versus M
// actually go -- is this call.
SimEngagementReport run_engagement(SimWorld &world, double max_seconds);

} // namespace defn

#endif
