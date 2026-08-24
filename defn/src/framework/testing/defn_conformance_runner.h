// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#ifndef DEFN_CONFORMANCE_RUNNER_H
#define DEFN_CONFORMANCE_RUNNER_H

#include "sim_world.h"
#include "unit_definition.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <string>
#include <vector>

namespace defn {

using namespace godot;

// The device that makes the simulator trustworthy: it runs one seeded scenario twice -- once as real Godot units in a
// real scene, once in the kernel -- and compares the traces.
//
// It has to be a node rather than a hosted test, because the real side needs actual frames: `Area2D` overlaps, which
// target selection reads, are only refreshed by the physics server between frames. The scenario is stepped by hand
// from `_process` so both sides advance by exactly the same fixed delta in exactly the same order.
//
// A failure means the kernel and the game disagree about a rule. That is a bug in one of them, whether or not anyone
// is running a sweep.
class DefnConformanceRunner : public Node {
    GDCLASS(DefnConformanceRunner, Node)

  public:
    void _process(double delta) override;

    [[nodiscard]] bool is_finished() const { return finished_; }
    [[nodiscard]] Dictionary get_result() const;

  protected:
    static void _bind_methods();

  private:
    struct Spawn {
        std::string unit_id;
        UnitSide side = UnitSide::FRIENDLY;
        Vector2 position;
    };

    struct Scenario {
        std::string name;
        std::vector<UnitConfig> roster;
        GlobalUnitConfig globals;
        std::vector<Spawn> spawns;
        int frames = 600;
    };

    // What both sides record, once every SAMPLE_INTERVAL_TICKS.
    struct Sample {
        int tick = 0;
        float x = 0.0F;
        int hp = 0;
        int pose = 0;
        int attack_mode = 0;
        bool engaged = false;
        bool alive = false;
    };

    struct Trace {
        std::vector<std::vector<Sample>> entities;
        std::vector<int> death_ticks;
        std::vector<int> projectile_counts;
    };

    bool load_content();
    void build_scenarios();
    void start_scenario();
    void step_game(double delta);
    void sample_game(int tick);
    void teardown_scenario();
    void run_kernel();
    void compare();
    [[nodiscard]] bool compare_entity(std::size_t index, const std::string &scenario_name);
    void compare_deaths(const std::string &scenario_name);
    void compare_projectiles(const std::string &scenario_name);

    std::vector<Scenario> scenarios_;
    std::size_t scenario_index_ = 0;
    int frame_ = 0;
    int warmup_frames_ = 0;
    bool scenario_active_ = false;
    bool finished_ = false;

    Node2D *entity_container_ = nullptr;
    // Units free themselves once their death fade finishes, so they are held by id rather than by pointer.
    std::vector<godot::ObjectID> game_entities_;
    Trace game_trace_;
    Trace kernel_trace_;

    std::vector<std::string> failures_;
    int compared_scenarios_ = 0;
};

} // namespace defn

#endif
