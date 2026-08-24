// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "defn_conformance_runner.h"

#include "animation_controller.h"
#include "combat_component.h"
#include "data_paths.h"
#include "godot_string.h"
#include "grid_manager.h"
#include "health_component.h"
#include "projectile_attack.h"
#include "sim_roster.h"
#include "unit.h"
#include "unit_data.h"
#include "unit_factory.h"

#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>
#include <format>

namespace defn {

namespace {

// Ten samples a second, the rate the plan starts at. Deaths and projectile counts are recorded every frame.
constexpr int SAMPLE_INTERVAL_TICKS = 6;
constexpr double FIXED_DELTA_SECONDS = 1.0 / 60.0;
// Positions accumulate in float on both sides; a pixel is far below anything that changes an outcome.
constexpr float POSITION_EPSILON = 1.0F;
constexpr int DEATH_TICK_TOLERANCE = 1;
constexpr float BELT_Y = 800.0F;

// Attack ranges vary per spawn in the shipped game. Pinning the variation removes the one place the two sides would
// have to draw identical random numbers, leaving the rules themselves as the only thing under test.
void pin_range_variation(UnitConfig &config) {
    config.melee_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    config.ranged_attack_range_variation = {.min = 1.0F, .max = 1.0F};
}

UnitConfig *find_config(std::vector<UnitConfig> &roster, const std::string &unit_id) {
    for (UnitConfig &config : roster) {
        if (config.name == unit_id) {
            return &config;
        }
    }

    return nullptr;
}

int to_int(UnitPose pose) { return static_cast<int>(pose); }
int to_int(AttackMode mode) { return static_cast<int>(mode); }

// A unit whose death fade has played out has already freed itself; resolving by id is the only safe way to look.
Unit *resolve_unit(godot::ObjectID object_id) {
    if (!object_id.is_valid()) {
        return nullptr;
    }
    auto *unit = Object::cast_to<Unit>(ObjectDB::get_instance(object_id));
    return unit != nullptr && !unit->is_queued_for_deletion() ? unit : nullptr;
}

} // namespace

void DefnConformanceRunner::_bind_methods() {
    ClassDB::bind_method(D_METHOD("is_finished"), &DefnConformanceRunner::is_finished);
    ClassDB::bind_method(D_METHOD("get_result"), &DefnConformanceRunner::get_result);
}

bool DefnConformanceRunner::load_content() {
    UnitDataLoader loader;
    if (!loader.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS)) {
        failures_.emplace_back("could not load unit data");
        return false;
    }

    GlobalUnitConfig globals = loader.get_globals();
    globals.melee_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    globals.ranged_attack_range_variation = {.min = 1.0F, .max = 1.0F};

    std::vector<UnitConfig> shipped;
    for (const char *unit_id : {"breacher", "marksman", "grime", "wrecker", "mason"}) {
        auto config = loader.get_unit(unit_id);
        if (!config) {
            failures_.emplace_back(std::string("missing unit: ") + unit_id);
            return false;
        }
        pin_range_variation(*config);
        shipped.push_back(*config);
    }

    // 1. Melee only. Ranged damage is stripped so the melee branch is the only one exercised.
    {
        Scenario scenario;
        scenario.name = "melee_only";
        scenario.globals = globals;
        scenario.roster = shipped;
        for (UnitConfig &config : scenario.roster) {
            config.ranged_damage = 0;
            config.projectile_attack.reset();
        }
        scenario.spawns = {
            {.unit_id = "breacher", .side = UnitSide::FRIENDLY, .position = {.x = 900.0F, .y = BELT_Y}},
            {.unit_id = "breacher", .side = UnitSide::FRIENDLY, .position = {.x = 820.0F, .y = BELT_Y}},
            {.unit_id = "grime", .side = UnitSide::HOSTILE, .position = {.x = 1000.0F, .y = BELT_Y}},
            {.unit_id = "grime", .side = UnitSide::HOSTILE, .position = {.x = 1080.0F, .y = BELT_Y}},
            {.unit_id = "grime", .side = UnitSide::HOSTILE, .position = {.x = 1160.0F, .y = BELT_Y}},
        };
        scenario.frames = 900;
        scenarios_.push_back(scenario);
    }

    // 2. Hitscan ranged, with no projectile config anywhere near the path.
    {
        Scenario scenario;
        scenario.name = "hitscan_ranged";
        scenario.globals = globals;
        scenario.roster = shipped;
        scenario.spawns = {
            {.unit_id = "marksman", .side = UnitSide::FRIENDLY, .position = {.x = 700.0F, .y = BELT_Y}},
            {.unit_id = "wrecker", .side = UnitSide::HOSTILE, .position = {.x = 1500.0F, .y = BELT_Y}},
            {.unit_id = "wrecker", .side = UnitSide::HOSTILE, .position = {.x = 1700.0F, .y = BELT_Y}},
        };
        scenario.frames = 900;
        scenarios_.push_back(scenario);
    }

    // 3. Projectile flight and splash into a cluster.
    {
        Scenario scenario;
        scenario.name = "projectile_splash";
        scenario.globals = globals;
        scenario.roster = shipped;
        scenario.spawns = {
            {.unit_id = "breacher", .side = UnitSide::FRIENDLY, .position = {.x = 700.0F, .y = BELT_Y}},
            {.unit_id = "breacher", .side = UnitSide::FRIENDLY, .position = {.x = 760.0F, .y = BELT_Y}},
            {.unit_id = "breacher", .side = UnitSide::FRIENDLY, .position = {.x = 820.0F, .y = BELT_Y}},
            {.unit_id = "mason", .side = UnitSide::HOSTILE, .position = {.x = 1100.0F, .y = BELT_Y}},
        };
        scenario.frames = 900;
        scenarios_.push_back(scenario);
    }

    // 4. Field promotion crossing its threshold mid-fight, which re-scales damage, attack period and max health.
    {
        Scenario scenario;
        scenario.name = "field_promotion";
        scenario.globals = globals;
        scenario.globals.field_promotion.damage_threshold = 120;
        scenario.roster = shipped;
        scenario.spawns = {
            {.unit_id = "breacher", .side = UnitSide::FRIENDLY, .position = {.x = 900.0F, .y = BELT_Y}},
            {.unit_id = "grime", .side = UnitSide::HOSTILE, .position = {.x = 1050.0F, .y = BELT_Y}},
            {.unit_id = "grime", .side = UnitSide::HOSTILE, .position = {.x = 1150.0F, .y = BELT_Y}},
            {.unit_id = "grime", .side = UnitSide::HOSTILE, .position = {.x = 1250.0F, .y = BELT_Y}},
        };
        scenario.frames = 1200;
        scenarios_.push_back(scenario);
    }

    // 5. A longer mixed engagement: every attack path at once, over twenty seconds.
    {
        Scenario scenario;
        scenario.name = "mixed_engagement";
        scenario.globals = globals;
        scenario.roster = shipped;
        scenario.spawns = {
            {.unit_id = "breacher", .side = UnitSide::FRIENDLY, .position = {.x = 600.0F, .y = BELT_Y}},
            {.unit_id = "marksman", .side = UnitSide::FRIENDLY, .position = {.x = 520.0F, .y = BELT_Y}},
            {.unit_id = "breacher", .side = UnitSide::FRIENDLY, .position = {.x = 680.0F, .y = BELT_Y}},
            {.unit_id = "grime", .side = UnitSide::HOSTILE, .position = {.x = 1300.0F, .y = BELT_Y}},
            {.unit_id = "wrecker", .side = UnitSide::HOSTILE, .position = {.x = 1420.0F, .y = BELT_Y}},
            {.unit_id = "mason", .side = UnitSide::HOSTILE, .position = {.x = 1560.0F, .y = BELT_Y}},
            {.unit_id = "grime", .side = UnitSide::HOSTILE, .position = {.x = 1680.0F, .y = BELT_Y}},
        };
        scenario.frames = 1200;
        scenarios_.push_back(scenario);
    }

    return true;
}

void DefnConformanceRunner::start_scenario() {
    const Scenario &scenario = scenarios_[scenario_index_];

    // GridManager supplies the friendly movement clamp, exactly as it does in a match.
    if (auto *grid = GridManager::get_singleton()) {
        grid->configure(scenario.globals.gameplay_rules, 0.66F, 0.825F);
    }

    entity_container_ = memnew(Node2D);
    entity_container_->set_name("ConformanceEntities");
    add_child(entity_container_);

    game_entities_.clear();
    game_trace_ = {};
    game_trace_.entities.resize(scenario.spawns.size());
    game_trace_.death_ticks.assign(scenario.spawns.size(), -1);

    std::vector<UnitConfig> roster = scenario.roster;
    for (const Spawn &spawn : scenario.spawns) {
        UnitConfig *config = find_config(roster, spawn.unit_id);
        if (config == nullptr) {
            failures_.push_back(scenario.name + ": unknown unit " + spawn.unit_id);
            continue;
        }

        UnitConfig spawn_config = *config;
        spawn_config.side = spawn.side;
        UnitRuntimeProfile profile = UnitRuntimeProfile::from_unit_config(spawn_config);
        profile.enable_sound = false;

        const ResolvedUnitRuntimeConfig resolved{
            .melee_attack_range = spawn_config.melee_attack_range,
            .ranged_attack_range = spawn_config.ranged_attack_range,
        };

        Unit *unit = UnitFactory::create(spawn_config, {spawn.position.x, spawn.position.y}, profile, resolved, scenario.globals.field_promotion);
        entity_container_->add_child(unit);
        UnitFactory::initialize(unit);

        // The scenario is stepped by hand, so nothing may also be stepped by the engine.
        if (auto *animation = Object::cast_to<AnimationController>(unit->get_node_or_null("AnimationController"))) {
            animation->set_process(false);
        }
        if (auto *combat = Object::cast_to<CombatComponent>(unit->get_node_or_null("CombatComponent"))) {
            combat->set_process(false);
        }

        game_entities_.emplace_back(unit->get_instance_id());
    }

    frame_ = 0;
    // The physics server only learns about the new detection areas on the next frame, and target selection reads
    // their overlaps. Letting the scene stand still for a frame or two means the run starts from a settled world
    // rather than one where nothing can see anything yet.
    warmup_frames_ = 2;
    scenario_active_ = true;
}

void DefnConformanceRunner::sample_game(int tick) {
    const auto &scenario = scenarios_[scenario_index_];
    for (std::size_t index = 0; index < game_entities_.size(); ++index) {
        Unit *unit = resolve_unit(game_entities_[index]);
        Sample sample;
        sample.tick = tick;
        if (unit == nullptr) {
            game_trace_.entities[index].emplace_back(sample);
            continue;
        }

        sample.alive = !unit->is_dead();
        sample.x = static_cast<float>(unit->get_global_position().x);
        sample.hp = unit->get_current_hp();
        if (auto *animation = Object::cast_to<AnimationController>(unit->get_node_or_null("AnimationController"))) {
            sample.pose = to_int(animation->get_anim_state());
        }
        if (auto *combat = Object::cast_to<CombatComponent>(unit->get_node_or_null("CombatComponent"))) {
            sample.engaged = combat->is_engaged();
            sample.attack_mode = to_int(combat->get_attack_mode());
        }
        game_trace_.entities[index].emplace_back(sample);
    }
    (void)scenario;

    int projectiles = 0;
    for (int child = 0; child < entity_container_->get_child_count(); ++child) {
        const auto *projectile = Object::cast_to<ProjectileAttack>(entity_container_->get_child(child));
        if (projectile != nullptr && projectile->is_in_flight()) {
            ++projectiles;
        }
    }
    game_trace_.projectile_counts.push_back(projectiles);
}

void DefnConformanceRunner::step_game(double delta) {
    // Deaths are watched every frame; positions and states are sampled at the trace rate.
    for (std::size_t index = 0; index < game_entities_.size(); ++index) {
        const Unit *unit = resolve_unit(game_entities_[index]);
        const bool dead = unit == nullptr || unit->is_dead();
        if (dead && game_trace_.death_ticks[index] < 0) {
            game_trace_.death_ticks[index] = frame_;
        }
    }

    if (frame_ % SAMPLE_INTERVAL_TICKS == 0) {
        sample_game(frame_);
    }

    // Shells that were already in the air before this frame's units ran. One launched during the loop below waits
    // until the next frame, because Godot walks a copy of the process group taken before the frame started.
    std::vector<godot::ObjectID> airborne;
    for (int child = 0; child < entity_container_->get_child_count(); ++child) {
        if (auto *projectile = Object::cast_to<ProjectileAttack>(entity_container_->get_child(child)); projectile != nullptr) {
            projectile->set_process(false);
            if (projectile->is_in_flight()) {
                airborne.emplace_back(projectile->get_instance_id());
            }
        }
    }

    // One frame of the shipped per-unit order: the animation controller runs before the combat component, and units
    // run in the order they were added to the container.
    for (const godot::ObjectID entity_id : game_entities_) {
        Unit *unit = resolve_unit(entity_id);
        if (unit == nullptr) {
            continue;
        }
        if (auto *animation = Object::cast_to<AnimationController>(unit->get_node_or_null("AnimationController"))) {
            animation->_process(delta);
        }
        if (auto *combat = Object::cast_to<CombatComponent>(unit->get_node_or_null("CombatComponent"))) {
            combat->_process(delta);
        }
    }

    // Then the projectiles, which the scene tree also reaches after every unit.
    for (const godot::ObjectID projectile_id : airborne) {
        auto *projectile = Object::cast_to<ProjectileAttack>(ObjectDB::get_instance(projectile_id));
        if (projectile != nullptr && !projectile->is_queued_for_deletion() && projectile->is_in_flight()) {
            projectile->_process(delta);
        }
    }

    ++frame_;
}

void DefnConformanceRunner::run_kernel() {
    const Scenario &scenario = scenarios_[scenario_index_];

    SimRoster roster;
    for (const UnitConfig &config : scenario.roster) {
        roster.add(config);
    }

    StdRandomSource random(1U);
    SimWorld world(roster, scenario.globals, random);

    kernel_trace_ = {};
    kernel_trace_.entities.resize(scenario.spawns.size());
    kernel_trace_.death_ticks.assign(scenario.spawns.size(), -1);

    std::vector<EntityId> ids;
    ids.reserve(scenario.spawns.size());
    for (const Spawn &spawn : scenario.spawns) {
        ids.push_back(world.spawn(spawn.unit_id, spawn.side, spawn.position).id);
    }
    world.begin_run();

    for (int tick = 0; tick < scenario.frames; ++tick) {
        for (std::size_t index = 0; index < ids.size(); ++index) {
            const SimEntity *entity = world.find_entity(ids[index]);
            if (entity != nullptr && entity->dead && kernel_trace_.death_ticks[index] < 0) {
                kernel_trace_.death_ticks[index] = tick;
            }
        }

        if (tick % SAMPLE_INTERVAL_TICKS == 0) {
            for (std::size_t index = 0; index < ids.size(); ++index) {
                const SimEntity *entity = world.find_entity(ids[index]);
                Sample sample;
                sample.tick = tick;
                if (entity != nullptr) {
                    sample.alive = !entity->dead;
                    sample.x = entity->position.x;
                    sample.hp = std::max(entity->hp, 0);
                    sample.pose = to_int(entity->animation.get_pose());
                    sample.engaged = entity->combat_state.engaged;
                    sample.attack_mode = to_int(entity->combat_state.attack_mode);
                }
                kernel_trace_.entities[index].emplace_back(sample);
            }
            kernel_trace_.projectile_counts.push_back(static_cast<int>(world.get_projectiles().size()));
        }

        world.tick();
    }
}

bool DefnConformanceRunner::compare_entity(std::size_t index, const std::string &scenario_name) {
    const auto fail = [this, &scenario_name](const std::string &message) { failures_.push_back(scenario_name + ": " + message); };
    const auto &game = game_trace_.entities[index];
    const auto &kernel = kernel_trace_.entities[index];

    if (game.size() != kernel.size()) {
        fail(std::format("entity {} sample count {} vs {}", index, game.size(), kernel.size()));
        return false;
    }

    for (std::size_t sample = 0; sample < game.size(); ++sample) {
        // Once either side reports the unit dead there is nothing left to agree about: the shipped node fades out and
        // is freed, while the kernel keeps a corpse.
        if (!game[sample].alive || !kernel[sample].alive) {
            return true;
        }

        const int tick = kernel[sample].tick;
        if (std::fabs(game[sample].x - kernel[sample].x) > POSITION_EPSILON) {
            fail(std::format("entity {} tick {} x {:.2f} vs {:.2f}", index, tick, game[sample].x, kernel[sample].x));
            return false;
        }
        if (game[sample].hp != kernel[sample].hp) {
            fail(std::format("entity {} tick {} hp {} vs {}", index, tick, game[sample].hp, kernel[sample].hp));
            return false;
        }
        if (game[sample].pose != kernel[sample].pose) {
            fail(std::format("entity {} tick {} pose {} vs {}", index, tick, game[sample].pose, kernel[sample].pose));
            return false;
        }
        if (game[sample].engaged != kernel[sample].engaged) {
            fail(std::format("entity {} tick {} engaged {} vs {}", index, tick, game[sample].engaged, kernel[sample].engaged));
            return false;
        }
        if (game[sample].attack_mode != kernel[sample].attack_mode) {
            fail(std::format("entity {} tick {} attack_mode {} vs {}", index, tick, game[sample].attack_mode, kernel[sample].attack_mode));
            return false;
        }
    }

    return true;
}

void DefnConformanceRunner::compare_deaths(const std::string &scenario_name) {
    const auto fail = [this, &scenario_name](const std::string &message) { failures_.push_back(scenario_name + ": " + message); };

    for (std::size_t index = 0; index < kernel_trace_.death_ticks.size(); ++index) {
        const int game_death = game_trace_.death_ticks[index];
        const int kernel_death = kernel_trace_.death_ticks[index];
        if ((game_death < 0) != (kernel_death < 0)) {
            fail(std::format("entity {} death disagreement: game {} vs kernel {}", index, game_death, kernel_death));
            continue;
        }
        if (game_death >= 0 && std::abs(game_death - kernel_death) > DEATH_TICK_TOLERANCE) {
            fail(std::format("entity {} died at tick {} vs {}", index, game_death, kernel_death));
        }
    }
}

void DefnConformanceRunner::compare_projectiles(const std::string &scenario_name) {
    const std::size_t samples = std::min(game_trace_.projectile_counts.size(), kernel_trace_.projectile_counts.size());
    for (std::size_t sample = 0; sample < samples; ++sample) {
        if (game_trace_.projectile_counts[sample] != kernel_trace_.projectile_counts[sample]) {
            failures_.push_back(scenario_name + ": " +
                                std::format("tick {} projectiles in flight {} vs {}", sample * SAMPLE_INTERVAL_TICKS, game_trace_.projectile_counts[sample],
                                            kernel_trace_.projectile_counts[sample]));
            return;
        }
    }
}

void DefnConformanceRunner::compare() {
    const std::string &scenario_name = scenarios_[scenario_index_].name;
    for (std::size_t index = 0; index < kernel_trace_.entities.size(); ++index) {
        (void)compare_entity(index, scenario_name);
    }
    compare_deaths(scenario_name);
    compare_projectiles(scenario_name);
    ++compared_scenarios_;
}

void DefnConformanceRunner::teardown_scenario() {
    if (entity_container_ != nullptr) {
        remove_child(entity_container_);
        memdelete(entity_container_);
        entity_container_ = nullptr;
    }
    game_entities_.clear();
    scenario_active_ = false;
}

void DefnConformanceRunner::_process(double delta) {
    if (finished_) {
        return;
    }

    if (scenarios_.empty()) {
        if (!load_content()) {
            finished_ = true;
            return;
        }
    }

    if (!scenario_active_) {
        if (scenario_index_ >= scenarios_.size()) {
            finished_ = true;
            return;
        }
        start_scenario();
        return;
    }

    if (warmup_frames_ > 0) {
        --warmup_frames_;
        return;
    }

    if (frame_ < scenarios_[scenario_index_].frames) {
        step_game(FIXED_DELTA_SECONDS);
        (void)delta;
        return;
    }

    run_kernel();
    compare();
    UtilityFunctions::print(String("[conformance] ") + to_godot_string(scenarios_[scenario_index_].name) + " compared");
    teardown_scenario();
    ++scenario_index_;
}

Dictionary DefnConformanceRunner::get_result() const {
    Array failures;
    for (const std::string &failure : failures_) {
        failures.push_back(to_godot_string(failure));
    }

    Dictionary result;
    result["success"] = failures_.empty() && compared_scenarios_ == static_cast<int>(scenarios_.size()) && !scenarios_.empty();
    result["scenarios"] = compared_scenarios_;
    result["failures"] = failures;
    return result;
}

} // namespace defn
