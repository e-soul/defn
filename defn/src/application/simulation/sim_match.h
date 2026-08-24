// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SIM_MATCH_H
#define SIM_MATCH_H

#include "level_definition.h"
#include "match_director.h"
#include "player_policy.h"
#include "sim_camera.h"
#include "sim_grid.h"
#include "sim_progression.h"
#include "sim_report.h"
#include "sim_scenario.h"
#include "sim_world.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace defn {

// A whole match, headless. `MatchDirector` runs the economy, waves, scoring and end conditions exactly as it does in
// game; `SimWorld` runs the fighting; this class is the composition root that replaces `GameManager` -- it applies
// spawn intents, asks the policy what to deploy, feeds deaths back as bounty, and keeps the base honest.
//
// The tick order mirrors the scene tree: the director runs first (Godot calls a parent before its children), then
// spawns land, then the player acts, then entities fight, then deaths are reported, then the camera moves.
class SimMatch {
  public:
    SimMatch(const UnitCatalog &catalog, const GlobalUnitConfig &globals, const LevelDefinition &level, const SimScenario &scenario,
             const std::vector<std::string> &base_unit_ids, const std::vector<ProgressionUpgradeCard> &upgrade_cards);

    // Runs to a decision or to the scenario's time limit, whichever comes first.
    SimMatchReport run();

  private:
    struct DeathRecord {
        EntityId id;
        UnitSide side = UnitSide::FRIENDLY;
        int bounty = 0;
    };

    struct UnitTally {
        int spawned = 0;
        int deaths = 0;
        int damage_dealt = 0;
        int damage_taken = 0;
        int kills = 0;
        double total_lifespan_seconds = 0.0;
    };

    void begin();
    void tick();
    void apply_match_update(const MatchUpdate &update);
    void run_policy();
    void report_deaths();
    void sample_metrics();
    [[nodiscard]] MatchObservation observe() const;
    [[nodiscard]] SimMatchReport build_report() const;
    [[nodiscard]] const SimEntity *find_base() const;

    SimScenario scenario_;
    LevelDefinition level_;
    StdRandomSource random_;
    SimGrid grid_;
    SimCamera camera_;
    SimProgression progression_;
    MatchDirector director_;
    SimWorld world_;
    std::unique_ptr<PlayerPolicy> policy_;
    std::vector<UnitConfig> roster_;

    EntityId base_id_;
    int current_wave_ = 1;
    std::optional<MatchSummaryModel> ending_summary_;
    double elapsed_seconds_ = 0.0;
    double energy_tick_accumulator_ = 0.0;
    double front_line_sample_accumulator_ = 0.0;
    bool finished_ = false;
    bool victory_ = false;
    double decided_at_seconds_ = 0.0;

    // Metrics
    double energy_idle_integral_ = 0.0;
    int peak_concurrent_enemies_ = 0;
    int energy_spent_ = 0;
    std::map<std::string, int> deployment_counts_;
    std::map<std::string, int> deployment_energy_;
    std::map<std::string, UnitTally> unit_tallies_;
    std::vector<float> front_line_trace_;
    std::vector<SimLeakEvent> leak_events_;
    std::vector<double> hostile_spawn_times_;
    std::vector<bool> death_reported_;
};

} // namespace defn

#endif
