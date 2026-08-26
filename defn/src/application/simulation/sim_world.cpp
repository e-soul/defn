// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "sim_world.h"

#include "unit_runtime_config_resolver.h"
#include "unit_runtime_profile.h"

#include <algorithm>
#include <vector>

namespace defn {

namespace {

// Mirrors UnitFactory::make_combat_config: a range of -1 tells classify_target_by_distance the unit has no attack of
// that kind, because every real distance is non-negative.
CombatConfig make_combat_config(const UnitConfig &config, const ResolvedUnitRuntimeConfig &resolved) {
    const bool has_melee_attack = config.melee_damage > 0;
    const bool has_ranged_attack = config.ranged_damage > 0 || config.projectile_attack.has_value();

    CombatConfig combat_config;
    combat_config.side = config.side;
    combat_config.melee_damage = config.melee_damage;
    combat_config.melee_attack_period_seconds = config.melee_attack_period_seconds;
    combat_config.ranged_damage = config.ranged_damage;
    combat_config.ranged_attack_period_seconds = config.ranged_attack_period_seconds;
    combat_config.attack_range = has_melee_attack ? resolved.melee_attack_range : -1.0F;
    combat_config.ranged_range = has_ranged_attack ? resolved.ranged_attack_range : -1.0F;
    combat_config.minimum_ranged_range = config.minimum_ranged_attack_range;
    combat_config.threat_weight = config.threat_weight;
    combat_config.target_preference = config.target_preference;
    combat_config.role = config.role;
    combat_config.role_bias = config.preferred_roles;
    combat_config.aggro_range = config.aggro_range;
    combat_config.melee_flash_color = config.melee_flash_color;
    combat_config.ranged_flash_color = config.ranged_flash_color;
    if (config.projectile_attack.has_value()) {
        combat_config.projectile_attack = to_projectile_damage_config(*config.projectile_attack);
    }
    return combat_config;
}

// Mirrors HealthComponent::take_damage, including the overkill cap: the return value is what actually landed.
int take_damage(SimEntity &entity, int amount) {
    amount = std::max(amount, 0);
    if (entity.hp <= 0 || amount == 0) {
        return 0;
    }
    // Armour is applied here rather than at the attacker so that every source pays it: melee, direct fire, and both
    // halves of a splash. Anywhere else and a shell would ignore the armour a rifle respects.
    amount = damage_after_armour(amount, entity.armour);

    const int previous_hp = entity.hp;
    entity.hp = std::max(entity.hp - amount, 0);
    return previous_hp - entity.hp;
}

} // namespace

SimWorld::SimWorld(const UnitCatalog &catalog, const GlobalUnitConfig &globals, RandomSource &random, const SimWorldConfig &config)
    : catalog_(catalog), globals_(globals), random_(random), config_(config) {
    const GameplayRules &rules = globals_.gameplay_rules;
    world_width_ = rules.viewport_width * static_cast<float>(rules.world_multiplier);
    friendly_world_margin_ = rules.friendly_world_margin;
}

SimSpawnResult SimWorld::spawn(const std::string &unit_id, UnitSide side, Vector2 position, const SimSpawnOverrides &overrides) {
    const std::optional<UnitConfig> config = catalog_.get_unit(unit_id);
    if (!config.has_value()) {
        return {.rejection = SimSpawnRejection::UNKNOWN_UNIT};
    }

    const UnitRuntimeProfile profile = UnitRuntimeProfile::from_unit_config(*config);
    const ResolvedUnitRuntimeConfig resolved = resolve_unit_runtime_config(to_runtime_range_config(*config), random_);

    SimEntity entity;
    entity.id = {.value = next_entity_id_++};
    entity.unit_id = unit_id;
    entity.side = side;
    entity.position = position;
    entity.hp = overrides.hp.value_or(config->hp);
    entity.max_hp = entity.hp;
    entity.armour = config->armour;
    entity.bounty = config->bounty;
    entity.spawn_tick = tick_index_;
    entity.spawn_time_seconds = elapsed_seconds_;
    entity.combat = make_combat_config(*config, resolved);
    entity.combat.side = side;
    // Mirrors the sensor radius in UnitFactory, and reads the *resolved catalog* range rather than the combat
    // config's, because a melee-only unit carries -1 there and would end up sensing nothing at all.
    entity.detection_radius = std::max(config->aggro_range, resolved.ranged_attack_range);
    entity.projectile_attack = config->projectile_attack;
    // AnimationController::get_muzzle_global_position resolves to owner->to_global(offset), and the unit's transform
    // carries its sprite scale.
    entity.muzzle_offset = {.x = config->muzzle.offset.x * config->scale, .y = config->muzzle.offset.y * config->scale};
    entity.move_speed_pixels_per_second = config->move_speed_pixels_per_second;
    entity.combat_enabled = profile.enable_combat;
    entity.movement_enabled = profile.enable_movement;
    entity.animation.configure(config->animations);
    entity.animation.set_pose(UnitPose::WALK); // AnimationController::configure ends the same way
    entity.field_promotion.configure(globals_.field_promotion, side == UnitSide::FRIENDLY && profile.enable_combat);

    entities_.push_back(std::move(entity));
    return {.id = entities_.back().id};
}

void SimWorld::tick() {
    // Anything created during this tick waits for the next one, because Godot walks a copy of the process group taken
    // before the frame started. Without this a unit deployed now would act a frame before its node ever could.
    const std::size_t projectiles_at_tick_start = projectiles_.size();

    // Ascending id is the scene-tree order the shipped game processes in: entities are appended in spawn order, and
    // Godot walks the process group depth-first over that same order.
    for (SimEntity &entity : entities_) {
        if (entity.dead || !entity.combat_enabled || entity.spawn_tick >= tick_index_) {
            continue;
        }
        step_entity(entity);
    }

    step_projectiles(projectiles_at_tick_start);
    ++tick_index_;
    elapsed_seconds_ += config_.fixed_delta_seconds;
}

void SimWorld::step_entity(SimEntity &entity) {
    // AnimationController::_process runs before CombatComponent::_process on the same unit, so combat always reads a
    // clock that has already taken this frame's step.
    entity.animation.advance(config_.fixed_delta_seconds);

    // CombatRuntime::update tries to release a committed shot before it does anything else, so a shot whose animation
    // reached its spawn frame this frame leaves the muzzle now rather than a frame late.
    launch_pending_projectile(entity);

    build_snapshots(entity);
    const CombatTargetSelection selection = select_target_from_snapshots(entity.position, entity.combat, entity.combat_state.target_id, snapshots_);
    if (selection.target_id.is_valid()) {
        entity.last_target_id = selection.target_id;
    }

    CombatLogicInput input;
    input.state = entity.combat_state;
    input.selection = selection;
    input.current_pose = to_combat_pose_state(entity.animation.get_pose());
    input.delta = config_.fixed_delta_seconds;
    input.unit_dead = entity.dead;
    input.projectile_pending = entity.pending_projectile.active;
    input.manual_repositioning = false;
    input.attack_animation_playing = entity.animation.is_attack_animation_playing();
    input.attack_windup_active = entity.animation.is_attack_windup_active();
    input.target_out_of_range = is_target_out_of_range(entity);

    const AdvanceCombatOutput output = advance_combat(entity.combat, input);
    entity.combat_state = output.state;
    apply_commands(entity, output.commands);

    // CombatRuntime::apply_commands tries again on the way out, so a shot whose spawn frame is 0 leaves on the same
    // frame it was decided.
    launch_pending_projectile(entity);
}

void SimWorld::build_snapshots(const SimEntity &viewer) {
    snapshots_.clear();

    // Replaces the Area2D overlap query: the detection sensor is a circle of the resolved ranged range around the unit.
    const float radius_squared = viewer.detection_radius * viewer.detection_radius;
    for (const SimEntity &other : entities_) {
        if (other.id == viewer.id || other.dead || other.side == viewer.side) {
            continue;
        }

        const float delta_x = other.position.x - viewer.position.x;
        const float delta_y = other.position.y - viewer.position.y;
        if ((delta_x * delta_x) + (delta_y * delta_y) > radius_squared) {
            continue;
        }

        snapshots_.push_back({.id = other.id,
                              .side = other.side,
                              .dead = other.dead,
                              .position = other.position,
                              .threat_weight = other.combat.threat_weight,
                              .health = other.hp,
                              .role = other.combat.role});
    }

    // A target that walked out of the sensor is still readable through its retained id, so CombatTargetSelector adds it
    // back. Without this the chase-versus-finish-the-backswing decision loses the target it is about to reason about.
    const EntityId current_target_id = viewer.combat_state.target_id;
    if (!current_target_id.is_valid()) {
        return;
    }
    const bool already_present =
        std::ranges::any_of(snapshots_, [current_target_id](const CombatTargetSnapshot &snapshot) { return snapshot.id == current_target_id; });
    if (already_present) {
        return;
    }
    if (const SimEntity *current_target = find_entity(current_target_id); current_target != nullptr) {
        snapshots_.push_back({
            .id = current_target->id,
            .side = current_target->side,
            .dead = current_target->dead,
            .position = current_target->position,
            .threat_weight = current_target->combat.threat_weight,
            .health = current_target->hp,
            .role = current_target->combat.role,
        });
    }
}

// Mirrors CombatRuntime::apply_command case for case. Muzzle flashes and damage flashes are presentation, so the
// kernel drops them; everything that changes the world is applied.
void SimWorld::apply_commands(SimEntity &entity, const std::vector<CombatCommand> &commands) {
    for (const CombatCommand &command : commands) {
        switch (command.type) {
        case CombatCommandType::STOP:
            // MovementComponent::stop() only exists to be overridden by presentation; it moves nothing.
            break;
        case CombatCommandType::MOVE:
            move(entity);
            break;
        case CombatCommandType::PLAY_POSE:
            apply_pose(entity, command.pose);
            break;
        case CombatCommandType::HIDE_MUZZLE_FLASH:
            break;
        case CombatCommandType::DEAL_DAMAGE:
            apply_damage(entity, command.target_id, command.damage);
            break;
        case CombatCommandType::SPAWN_PROJECTILE:
            entity.pending_projectile = {
                .active = true,
                .target_id = command.target_id,
                .target_position = command.target_position,
                .fallback_damage = command.damage,
            };
            break;
        case CombatCommandType::PLAY_EFFECT:
            if (command.effect == CombatEffectType::MELEE_ATTACK) {
                entity.animation.play_attack();
            } else if (command.effect == CombatEffectType::RANGED_SHOOT) {
                if (entity.pending_projectile.active && entity.projectile_attack.has_value()) {
                    // The shot waits for its spawn frame, and its shooter is frozen until then.
                    entity.animation.play_shoot(entity.projectile_attack->spawn_animation_frame);
                } else {
                    // Hitscan fire: the shot has already been resolved, so the effect is released and consumed at once.
                    entity.animation.play_shoot(0);
                    (void)entity.animation.consume_shoot_effect_triggered();
                }
            }
            break;
        }
    }
}

void SimWorld::apply_pose(SimEntity &entity, CombatPoseIntent pose) {
    switch (pose) {
    case CombatPoseIntent::WALK:
        entity.animation.set_pose(UnitPose::WALK);
        break;
    case CombatPoseIntent::ATTACK:
        entity.animation.hold_pose(UnitPose::ATTACK);
        break;
    case CombatPoseIntent::SHOOT:
        entity.animation.hold_pose(UnitPose::SHOOT);
        break;
    case CombatPoseIntent::NONE:
        break;
    }
}

// Mirrors CombatAttackExecutor::spawn_pending_projectile: the shot is held back until the shoot animation reaches the
// frame that releases it, and the target position it flies to was frozen when combat committed to the shot.
void SimWorld::launch_pending_projectile(SimEntity &shooter) {
    if (!shooter.pending_projectile.active) {
        return;
    }
    if (!shooter.animation.consume_shoot_effect_triggered()) {
        return;
    }

    if (shooter.projectile_attack.has_value()) {
        const Vector2 muzzle{.x = shooter.position.x + shooter.muzzle_offset.x, .y = shooter.position.y + shooter.muzzle_offset.y};
        projectiles_.push_back({
            .id = {.value = next_projectile_id_++},
            .source_id = shooter.id,
            .direct_target_id = shooter.pending_projectile.target_id,
            .shooter_side = shooter.combat.side,
            .damage_config = to_projectile_damage_config(*shooter.projectile_attack),
            .fallback_damage = shooter.pending_projectile.fallback_damage,
            .flight = begin_projectile_flight(muzzle, shooter.pending_projectile.target_position, shooter.projectile_attack->speed_pixels_per_second),
        });

        ++shooter.projectiles_launched;

        // A shot with nowhere to travel detonates on the spot, before anything else gets a turn.
        if (projectile_arrives_immediately(projectiles_.back().flight)) {
            detonate(projectiles_.back());
        }
    }

    shooter.pending_projectile = {};
}

// Projectiles process after every unit: ProjectileAttack nodes are appended to the entity container, and Godot walks
// the process group in tree order.
void SimWorld::step_projectiles(std::size_t count_at_tick_start) {
    for (std::size_t index = 0; index < count_at_tick_start; ++index) {
        SimProjectile &projectile = projectiles_[index];
        if (projectile.exploded) {
            continue;
        }

        if (advance_projectile(projectile.flight, config_.fixed_delta_seconds).arrived) {
            detonate(projectile);
        }
    }

    // A shot that has gone off no longer affects the world, so it is dropped rather than kept around for the explosion
    // animation the shipped game still has to play out.
    std::erase_if(projectiles_, [](const SimProjectile &projectile) { return projectile.exploded; });
}

// Mirrors ProjectileAttack::apply_splash_damage.
void SimWorld::detonate(SimProjectile &projectile) {
    projectile.exploded = true;
    build_impact_snapshots(projectile.direct_target_id);

    const std::vector<ProjectileDamageCommand> commands = resolve_projectile_impact({
        .config = projectile.damage_config,
        .shooter_side = projectile.shooter_side,
        .impact_position = projectile.flight.target_position,
        .direct_target_id = projectile.direct_target_id,
        .fallback_damage = projectile.fallback_damage,
        .targets = impact_snapshots_,
    });

    SimEntity *source = find_mutable_entity(projectile.source_id);
    if (source == nullptr) {
        return;
    }

    for (const ProjectileDamageCommand &command : commands) {
        const SimEntity *victim = find_entity(command.target_id);
        if (victim == nullptr || victim->dead) {
            continue;
        }
        apply_damage(*source, command.target_id, command.damage);
    }
}

// The shipped game gathers every AttackTarget under the projectile's parent, with the direct target listed first. The
// order matters: resolve_projectile_impact trims its candidate list from the back.
void SimWorld::build_impact_snapshots(EntityId direct_target_id) {
    impact_snapshots_.clear();

    const auto push = [this](const SimEntity &entity) {
        impact_snapshots_.push_back({.id = entity.id, .side = entity.side, .dead = entity.dead, .position = entity.position});
    };

    if (const SimEntity *direct_target = find_entity(direct_target_id); direct_target != nullptr) {
        push(*direct_target);
    }

    for (const SimEntity &entity : entities_) {
        if (entity.id == direct_target_id) {
            continue;
        }
        push(entity);
    }
}

// Mirrors MovementComponent::move.
void SimWorld::move(SimEntity &entity) const {
    if (!entity.movement_enabled || entity.move_speed_pixels_per_second <= 0.0F || config_.fixed_delta_seconds <= 0.0) {
        return;
    }

    const float displacement = entity.move_speed_pixels_per_second * static_cast<float>(config_.fixed_delta_seconds);
    if (entity.side == UnitSide::FRIENDLY) {
        const float max_x = world_width_ - friendly_world_margin_;
        if (entity.position.x < max_x) {
            entity.position.x = std::min(entity.position.x + displacement, max_x);
        }
        return;
    }

    entity.position.x -= displacement;
}

// Mirrors DamageDispatcher::apply plus the death handling GameManager wires up through the "died" signal.
void SimWorld::apply_damage(SimEntity &source, EntityId target_id, int base_damage) {
    SimEntity *target = find_mutable_entity(target_id);
    if (target == nullptr) {
        return;
    }

    const bool live_friendly_source = !source.dead && source.side == UnitSide::FRIENDLY;
    const int resolved_damage = live_friendly_source ? source.field_promotion.outgoing_damage(base_damage) : base_damage;
    const int effective_damage = take_damage(*target, resolved_damage);
    if (effective_damage <= 0) {
        return;
    }

    source.damage_dealt += effective_damage;
    ++source.attacks_landed;
    target->damage_taken += effective_damage;

    if (live_friendly_source && source.side != target->side) {
        record_effective_damage_dealt(source, effective_damage);
    }

    const bool lethal = target->hp <= 0 && !target->dead;
    if (lethal) {
        target->dead = true;
        target->death_time_seconds = elapsed_seconds_;
        target->animation.set_pose(UnitPose::DEATH);
        ++source.kills;
    }

    damage_events_.push_back({
        .time_seconds = elapsed_seconds_,
        .source_id = source.id,
        .target_id = target->id,
        .damage = effective_damage,
        .lethal = lethal,
    });
}

std::vector<SimDamageEvent> SimWorld::drain_damage_events() {
    std::vector<SimDamageEvent> drained;
    drained.swap(damage_events_);
    return drained;
}

// Mirrors Unit::record_effective_damage_dealt: a promotion re-scales the attack periods and heals to the new maximum.
void SimWorld::record_effective_damage_dealt(SimEntity &source, int effective_damage) {
    const FieldPromotionUpdate update = source.field_promotion.record_effective_damage(effective_damage);
    if (!update.promotion_granted) {
        return;
    }

    const FieldPromotionRules &rules = source.field_promotion.get_rules();
    source.combat.melee_attack_period_seconds = apply_promoted_attack_period(source.combat.melee_attack_period_seconds, rules);
    source.combat.ranged_attack_period_seconds = apply_promoted_attack_period(source.combat.ranged_attack_period_seconds, rules);
    source.max_hp = apply_promoted_max_health(source.max_hp, rules);
    source.hp = source.max_hp;
}

// Mirrors CombatTargetSelector::is_target_out_of_range.
bool SimWorld::is_target_out_of_range(const SimEntity &viewer) const {
    const SimEntity *target = find_entity(viewer.last_target_id);
    if (target == nullptr || target->dead || target->side == viewer.combat.side) {
        return false;
    }

    const float distance = get_forward_distance(viewer.combat.side, viewer.position, target->position);
    return classify_target_by_distance(viewer.combat, distance) == AttackMode::NONE;
}

const SimEntity *SimWorld::find_entity(EntityId entity_id) const {
    if (!entity_id.is_valid() || entity_id.value >= next_entity_id_) {
        return nullptr;
    }

    // Ids are dense and assigned in append order, so the id is the index plus one.
    return &entities_[static_cast<std::size_t>(entity_id.value - 1)];
}

SimEntity *SimWorld::find_mutable_entity(EntityId entity_id) {
    if (!entity_id.is_valid() || entity_id.value >= next_entity_id_) {
        return nullptr;
    }

    return &entities_[static_cast<std::size_t>(entity_id.value - 1)];
}

SimSideSummary SimWorld::summarize(UnitSide side) const {
    SimSideSummary summary;
    for (const SimEntity &entity : entities_) {
        if (entity.side != side) {
            continue;
        }
        summary.damage_dealt += entity.damage_dealt;
        if (!entity.dead) {
            ++summary.alive;
            summary.hp_remaining += entity.hp;
        }
    }

    return summary;
}

int SimWorld::count_alive(UnitSide side) const { return summarize(side).alive; }

SimEngagementReport run_engagement(SimWorld &world, double max_seconds) {
    SimEngagementReport report;

    while (world.get_elapsed_seconds() < max_seconds) {
        if (world.count_alive(UnitSide::FRIENDLY) == 0 || world.count_alive(UnitSide::HOSTILE) == 0) {
            break;
        }
        world.tick();
    }

    report.duration_seconds = world.get_elapsed_seconds();
    report.friendly = world.summarize(UnitSide::FRIENDLY);
    report.hostile = world.summarize(UnitSide::HOSTILE);
    report.resolved = report.friendly.alive == 0 || report.hostile.alive == 0;
    if (report.resolved) {
        if (report.friendly.alive > 0) {
            report.winner = UnitSide::FRIENDLY;
        } else if (report.hostile.alive > 0) {
            report.winner = UnitSide::HOSTILE;
        }
    }

    return report;
}

} // namespace defn
