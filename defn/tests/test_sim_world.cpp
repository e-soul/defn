// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "test_harness.h"

#include "sim_roster.h"
#include "sim_world.h"

namespace defn {

namespace {

constexpr float BELT_Y = 800.0F;

// The shipped animation shape: ten frames at ten frames per second, the first three of an attack committed.
std::vector<std::pair<std::string, AnimConfig>> make_standard_animations() {
    const AnimConfig looping{.path_template = "", .frame_count = 10, .speed = 10.0, .loop = true, .windup_frames = 0};
    const AnimConfig one_shot{.path_template = "", .frame_count = 10, .speed = 10.0, .loop = false, .windup_frames = 3};
    return {{"walk", looping}, {"attack", one_shot}, {"shoot", one_shot}, {"death", one_shot}};
}

// A unit with no range variation, so every distance in a test is exactly the number it says.
UnitConfig make_unit(const std::string &name, UnitSide side, int hp, int melee_damage) {
    UnitConfig config;
    config.name = name;
    config.side = side;
    config.hp = hp;
    config.melee_damage = melee_damage;
    config.melee_attack_period_seconds = 1.0;
    config.melee_attack_range = 100.0F;
    config.melee_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    config.ranged_damage = 0;
    config.ranged_attack_range = 500.0F;
    config.ranged_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    config.move_speed_pixels_per_second = 0.0F;
    config.animations = make_standard_animations();
    return config;
}

// An inert target: no attacks and no legs, so it only ever soaks damage.
UnitConfig make_dummy(const std::string &name, UnitSide side, int hp) { return make_unit(name, side, hp, 0); }

// A shooter that lobs a shell instead of hitting instantly.
UnitConfig make_lobber(const std::string &name, UnitSide side, const ProjectileAttackConfig &projectile) {
    UnitConfig config = make_unit(name, side, 100, 0);
    config.ranged_damage = 10;
    config.ranged_attack_period_seconds = 1.0;
    config.ranged_attack_range = 400.0F;
    config.projectile_attack = projectile;
    return config;
}

int total_damage_taken(const SimWorld &world, UnitSide side) {
    int damage = 0;
    for (const SimEntity &entity : world.get_entities()) {
        if (entity.side == side) {
            damage += entity.damage_taken;
        }
    }

    return damage;
}

GlobalUnitConfig make_globals() {
    GlobalUnitConfig globals;
    globals.melee_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    globals.ranged_attack_range_variation = {.min = 1.0F, .max = 1.0F};
    return globals;
}

// The shipped roster.
SimRoster make_shipped_roster() {
    SimRoster roster;

    UnitConfig breacher = make_unit("breacher", UnitSide::FRIENDLY, 400, 15);
    breacher.melee_attack_range = 128.0F;
    breacher.ranged_damage = 8;
    breacher.ranged_attack_period_seconds = 0.72;
    breacher.ranged_attack_range = 245.0F;
    breacher.move_speed_pixels_per_second = 58.0F;
    roster.add(breacher);

    UnitConfig marksman = make_unit("marksman", UnitSide::FRIENDLY, 180, 15);
    marksman.melee_attack_range = 128.0F;
    marksman.ranged_damage = 19;
    marksman.ranged_attack_period_seconds = 1.05;
    marksman.ranged_attack_range = 650.0F;
    marksman.move_speed_pixels_per_second = 74.0F;
    roster.add(marksman);

    UnitConfig grime = make_unit("grime", UnitSide::HOSTILE, 95, 15);
    grime.melee_attack_range = 128.0F;
    grime.ranged_damage = 5;
    grime.ranged_attack_period_seconds = 0.62;
    grime.ranged_attack_range = 345.0F;
    grime.move_speed_pixels_per_second = 72.0F;
    roster.add(grime);

    UnitConfig mason = make_unit("mason", UnitSide::HOSTILE, 82, 15);
    mason.melee_attack_range = 128.0F;
    mason.ranged_damage = 10;
    mason.ranged_attack_period_seconds = 1.0;
    mason.ranged_attack_range = 400.0F;
    mason.move_speed_pixels_per_second = 48.0F;
    mason.projectile_attack = ProjectileAttackConfig{
        .speed_pixels_per_second = 1800.0F,
        .splash_radius = 140.0F,
        .affected_fraction = 1.0F,
        .min_affected_targets = 1,
        .spawn_animation_frame = 2,
        .affected_target_rounding = SplashTargetRoundingMode::NEAREST,
        .include_direct_target = true,
        .impact_damage = 10,
        .splash_damage = 12,
    };
    roster.add(mason);

    return roster;
}

// A shooter that reaches both hostiles from where it stands, so target selection has a real choice on the first
// tick rather than acquiring whatever wanders into range first.
SimRoster make_preference_roster(TargetPreference preference, float defender_threat_weight) {
    SimRoster roster;

    UnitConfig sniper = make_unit("sniper", UnitSide::FRIENDLY, 500, 0);
    sniper.ranged_damage = 20;
    sniper.ranged_attack_period_seconds = 1.0;
    sniper.ranged_attack_range = 900.0F;
    sniper.move_speed_pixels_per_second = 0.0F;
    sniper.target_preference = preference;
    roster.add(sniper);

    UnitConfig defender = make_dummy("defender", UnitSide::HOSTILE, 300);
    defender.threat_weight = defender_threat_weight;
    roster.add(defender);

    UnitConfig backline = make_dummy("backline", UnitSide::HOSTILE, 100);
    roster.add(backline);

    return roster;
}

std::string targeted_unit_id(SimWorld &world, EntityId target_id) {
    for (const SimEntity &entity : world.get_entities()) {
        if (entity.id == target_id) {
            return entity.unit_id;
        }
    }
    return "none";
}

// Runs one tick and reports what the sniper reached for. The point is the wiring, not the fight: UnitConfig ->
// CombatConfig -> snapshot -> selection has three hand-offs and any one of them silently dropping the field would
// leave a unit playing by the default rule while the catalog says otherwise.
std::string first_target_of_sniper(SimRoster &roster) {
    StdRandomSource random(1U);
    SimWorld world(roster, make_globals(), random);
    world.spawn("sniper", UnitSide::FRIENDLY, {.x = 0.0F, .y = BELT_Y});
    world.spawn("defender", UnitSide::HOSTILE, {.x = 300.0F, .y = BELT_Y});
    world.spawn("backline", UnitSide::HOSTILE, {.x = 700.0F, .y = BELT_Y});
    world.begin_run();
    world.tick();

    for (const SimEntity &entity : world.get_entities()) {
        if (entity.unit_id == "sniper") {
            return targeted_unit_id(world, entity.combat_state.target_id);
        }
    }
    return "none";
}

} // namespace

DEFN_TEST(sim_world_targets_the_nearest_enemy_by_default) {
    SimRoster roster = make_preference_roster(TargetPreference::NEAREST, 1.0F);

    DEFN_CHECK_EQ(first_target_of_sniper(roster), std::string("defender"));
}

DEFN_TEST(sim_world_carries_a_farthest_preference_from_the_catalog) {
    SimRoster roster = make_preference_roster(TargetPreference::FARTHEST, 1.0F);

    DEFN_CHECK_EQ(first_target_of_sniper(roster), std::string("backline"));
}

DEFN_TEST(sim_world_carries_a_highest_hp_preference_from_the_catalog) {
    // The defender has 300 hp to the backline's 100, and is also the nearer of the two: the preference has to be the
    // reason it is chosen, so this pairs with the farthest case above to rule out "nearest happened to win".
    SimRoster roster = make_preference_roster(TargetPreference::HIGHEST_HP, 1.0F);

    DEFN_CHECK_EQ(first_target_of_sniper(roster), std::string("defender"));
}

DEFN_TEST(sim_world_carries_a_threat_weight_from_the_catalog) {
    // Farthest would reach past the defender; a heavy enough defender pulls the shot back onto itself.
    SimRoster roster = make_preference_roster(TargetPreference::FARTHEST, 4.0F);

    DEFN_CHECK_EQ(first_target_of_sniper(roster), std::string("defender"));
}

// The other direction, which is what the base's `threat_weight: 0.25` relies on and which nothing exercised before:
// a weight below one has to make a target *less* attractive than plain geometry says it is. Nearest would take the
// defender at 300 over the backline at 700; at quarter weight the defender scores 1200 and loses to the backline's
// 700. A multiplier that silently clamped at one would leave the tower a normal candidate and this test would fail.
DEFN_TEST(sim_world_carries_a_threat_weight_below_one_from_the_catalog) {
    SimRoster roster = make_preference_roster(TargetPreference::NEAREST, 0.25F);

    DEFN_CHECK_EQ(first_target_of_sniper(roster), std::string("backline"));
}

namespace {

// Armour reaches the kernel's damage path from the catalog, and applies to every source rather than only to shots.
int damage_dealt_to_armoured_dummy(int shot_damage, int armour) {
    SimRoster roster;
    UnitConfig shooter = make_unit("shooter", UnitSide::FRIENDLY, 400, 0);
    shooter.ranged_damage = shot_damage;
    shooter.ranged_attack_period_seconds = 1.0;
    shooter.ranged_attack_range = 500.0F;
    shooter.move_speed_pixels_per_second = 0.0F;
    roster.add(shooter);

    UnitConfig dummy = make_dummy("dummy", UnitSide::HOSTILE, 5000);
    dummy.armour = armour;
    dummy.move_speed_pixels_per_second = 0.0F;
    roster.add(dummy);

    StdRandomSource random(1U);
    SimWorld world(roster, make_globals(), random);
    world.spawn("shooter", UnitSide::FRIENDLY, {.x = 0.0F, .y = BELT_Y});
    world.spawn("dummy", UnitSide::HOSTILE, {.x = 300.0F, .y = BELT_Y});
    world.begin_run();
    run_engagement(world, 12.0);

    return total_damage_taken(world, UnitSide::HOSTILE);
}

} // namespace

DEFN_TEST(sim_world_applies_armour_from_the_catalog) {
    const int bare = damage_dealt_to_armoured_dummy(19, 0);
    const int armoured = damage_dealt_to_armoured_dummy(19, 6);

    DEFN_CHECK(bare > 0);
    // Same number of shots either way -- the shooter is stationary and the dummy cannot die -- so the ratio is the
    // per-shot reduction: 13 of every 19.
    DEFN_CHECK_EQ(armoured * 19, bare * 13);
}

DEFN_TEST(sim_world_never_lets_armour_block_a_shot_completely) {
    const int chipped = damage_dealt_to_armoured_dummy(4, 50);

    DEFN_CHECK(chipped > 0);
}

namespace {

// Does a minimum range from the catalog actually reach the kernel's range gate? The rule is tested directly
// elsewhere; this asks whether the plumbing carries it, which is a different question and the one that bit before.
int damage_dealt_by_shooter_with_dead_zone(float minimum_range, float target_distance) {
    SimRoster roster;
    UnitConfig shooter = make_unit("shooter", UnitSide::FRIENDLY, 400, 0);
    shooter.ranged_damage = 15;
    shooter.ranged_attack_period_seconds = 1.0;
    shooter.ranged_attack_range = 800.0F;
    shooter.minimum_ranged_attack_range = minimum_range;
    shooter.melee_damage = 0; // no fallback punch, so the dead zone is the whole story
    shooter.move_speed_pixels_per_second = 0.0F;
    roster.add(shooter);

    UnitConfig dummy = make_dummy("dummy", UnitSide::HOSTILE, 4000);
    dummy.move_speed_pixels_per_second = 0.0F;
    roster.add(dummy);

    StdRandomSource random(1U);
    SimWorld world(roster, make_globals(), random);
    world.spawn("shooter", UnitSide::FRIENDLY, {.x = 0.0F, .y = BELT_Y});
    world.spawn("dummy", UnitSide::HOSTILE, {.x = target_distance, .y = BELT_Y});
    world.begin_run();
    run_engagement(world, 10.0);

    return total_damage_taken(world, UnitSide::HOSTILE);
}

} // namespace

DEFN_TEST(sim_world_carries_a_minimum_range_from_the_catalog) {
    // Same target, same shooter, same distance: only the dead zone differs.
    const int without_dead_zone = damage_dealt_by_shooter_with_dead_zone(0.0F, 150.0F);
    const int inside_dead_zone = damage_dealt_by_shooter_with_dead_zone(300.0F, 150.0F);
    const int outside_dead_zone = damage_dealt_by_shooter_with_dead_zone(300.0F, 500.0F);

    DEFN_CHECK(without_dead_zone > 0);
    DEFN_CHECK_EQ(inside_dead_zone, 0); // too close to shoot, and no melee to fall back on
    DEFN_CHECK(outside_dead_zone > 0);
}

namespace {

// The pursuit gate, end to end. The rule is tested directly in test_combat_logic; this asks whether the catalog
// reaches it -- UnitConfig -> CombatConfig -> sensor radius -> snapshot -> selection -> movement is five hand-offs, and
// a role that got dropped at any one of them would leave the unit stopping at the first thing it can shoot while the
// catalog says otherwise. The observable is deliberately the shooter's *position*: nothing else proves the declined
// target turned into forward movement rather than into standing still.
float shooter_x_after_pursuit(float sniper_bias) {
    SimRoster roster;
    UnitConfig shooter = make_unit("shooter", UnitSide::FRIENDLY, 500, 0);
    shooter.ranged_damage = 20;
    shooter.ranged_attack_period_seconds = 1.0;
    shooter.ranged_attack_range = 400.0F; // reaches the blocker at 300, not the sniper at 700
    shooter.aggro_range = 800.0F;         // but senses the sniper, which is the gap pursuit lives in
    shooter.melee_attack_range = 0.0F;
    shooter.move_speed_pixels_per_second = 100.0F;
    shooter.preferred_roles.fill(1.0F);
    shooter.preferred_roles.at(static_cast<std::size_t>(unit_role_index(UnitRole::SNIPER))) = sniper_bias;
    roster.add(shooter);

    UnitConfig blocker = make_dummy("blocker", UnitSide::HOSTILE, 100000);
    blocker.role = UnitRole::TANK;
    roster.add(blocker);

    UnitConfig sniper = make_dummy("sniper", UnitSide::HOSTILE, 100000);
    sniper.role = UnitRole::SNIPER;
    roster.add(sniper);

    StdRandomSource random(1U);
    SimWorld world(roster, make_globals(), random);
    world.spawn("shooter", UnitSide::FRIENDLY, {.x = 0.0F, .y = BELT_Y});
    world.spawn("blocker", UnitSide::HOSTILE, {.x = 300.0F, .y = BELT_Y});
    world.spawn("sniper", UnitSide::HOSTILE, {.x = 700.0F, .y = BELT_Y});
    world.begin_run();
    run_engagement(world, 1.0);

    for (const SimEntity &entity : world.get_entities()) {
        if (entity.unit_id == "shooter") {
            return entity.position.x;
        }
    }
    return -1.0F;
}

} // namespace

DEFN_TEST(sim_world_carries_a_role_preference_and_aggro_range_from_the_catalog) {
    const float held_by_the_blocker = shooter_x_after_pursuit(1.0F);
    const float walked_past_it = shooter_x_after_pursuit(3.0F);

    // Same field, same ranges, same second of simulation: only the bias differs.
    DEFN_CHECK(walked_past_it > held_by_the_blocker);
    DEFN_CHECK(held_by_the_blocker < 1.0F); // stopped on the target it could already shoot
    DEFN_CHECK(walked_past_it > 50.0F);     // declined it and kept closing on the sniper
}

DEFN_TEST(sim_world_refuses_to_spawn_an_unknown_unit) {
    SimRoster roster = make_shipped_roster();
    StdRandomSource random(1U);
    SimWorld world(roster, make_globals(), random);

    const SimSpawnResult result = world.spawn("nosuchunit", UnitSide::FRIENDLY, {.x = 0.0F, .y = BELT_Y});

    DEFN_CHECK(!result.succeeded());
    DEFN_CHECK_EQ(static_cast<int>(result.rejection), static_cast<int>(SimSpawnRejection::UNKNOWN_UNIT));
    DEFN_CHECK_EQ(static_cast<int>(world.get_entities().size()), 0);
}

DEFN_TEST(sim_world_spawns_a_unit_that_shoots_projectiles) {
    SimRoster roster = make_shipped_roster();
    StdRandomSource random(1U);
    SimWorld world(roster, make_globals(), random);

    const SimSpawnResult result = world.spawn("mason", UnitSide::HOSTILE, {.x = 0.0F, .y = BELT_Y});

    DEFN_REQUIRE(result.succeeded());
    const SimEntity *mason = world.find_entity(result.id);
    DEFN_REQUIRE(mason != nullptr);
    DEFN_CHECK(mason->projectile_attack.has_value());
}

DEFN_TEST(sim_world_resolves_a_melee_duel_by_the_numbers) {
    SimRoster roster;
    roster.add(make_unit("brute", UnitSide::FRIENDLY, 100, 10));
    roster.add(make_unit("runt", UnitSide::HOSTILE, 30, 5));

    StdRandomSource random(7U);
    SimWorld world(roster, make_globals(), random);
    const SimSpawnResult brute = world.spawn("brute", UnitSide::FRIENDLY, {.x = 0.0F, .y = BELT_Y});
    const SimSpawnResult runt = world.spawn("runt", UnitSide::HOSTILE, {.x = 50.0F, .y = BELT_Y});
    DEFN_REQUIRE(brute.succeeded());
    DEFN_REQUIRE(runt.succeeded());

    const SimEngagementReport report = run_engagement(world, 60.0);

    DEFN_CHECK(report.resolved);
    DEFN_REQUIRE(report.winner.has_value());
    DEFN_CHECK_EQ(static_cast<int>(*report.winner), static_cast<int>(UnitSide::FRIENDLY));

    // Three blows of ten kill the runt; it lands two before dying, and never acts on the tick it dies.
    const SimEntity *brute_entity = world.find_entity(brute.id);
    const SimEntity *runt_entity = world.find_entity(runt.id);
    DEFN_REQUIRE(brute_entity != nullptr);
    DEFN_REQUIRE(runt_entity != nullptr);
    DEFN_CHECK_EQ(brute_entity->hp, 90);
    DEFN_CHECK_EQ(brute_entity->attacks_landed, 3);
    DEFN_CHECK_EQ(brute_entity->kills, 1);
    DEFN_CHECK_EQ(brute_entity->damage_dealt, 30);
    DEFN_CHECK(runt_entity->dead);
    DEFN_CHECK_EQ(runt_entity->attacks_landed, 2);
}

DEFN_TEST(sim_world_lets_a_unit_walk_into_range_before_it_engages) {
    SimRoster roster;
    UnitConfig walker = make_unit("walker", UnitSide::FRIENDLY, 100, 10);
    walker.move_speed_pixels_per_second = 100.0F;
    walker.ranged_attack_range = 150.0F; // keeps the sensor short so the approach is visible
    roster.add(walker);
    roster.add(make_unit("post", UnitSide::HOSTILE, 1000, 0));

    StdRandomSource random(3U);
    SimWorld world(roster, make_globals(), random);
    const SimSpawnResult mover = world.spawn("walker", UnitSide::FRIENDLY, {.x = 0.0F, .y = BELT_Y});
    world.spawn("post", UnitSide::HOSTILE, {.x = 400.0F, .y = BELT_Y});

    for (int tick = 0; tick < 60; ++tick) {
        world.tick();
    }

    const SimEntity *walker_entity = world.find_entity(mover.id);
    DEFN_REQUIRE(walker_entity != nullptr);
    // One second at 100 px/s, less the frame it spent waiting to join the process group.
    DEFN_CHECK_CLOSE(walker_entity->position.x, 98.3, 0.5);
    DEFN_CHECK_EQ(walker_entity->attacks_landed, 0);

    for (int tick = 0; tick < 180; ++tick) {
        world.tick();
    }

    DEFN_CHECK(walker_entity->attacks_landed > 0);
    DEFN_CHECK(walker_entity->position.x <= 400.0F);
}

DEFN_TEST(sim_world_holds_friendlies_at_the_world_margin) {
    SimRoster roster;
    UnitConfig walker = make_unit("walker", UnitSide::FRIENDLY, 100, 10);
    walker.move_speed_pixels_per_second = 4000.0F;
    roster.add(walker);

    GlobalUnitConfig globals = make_globals();
    globals.gameplay_rules.viewport_width = 1000.0F;
    globals.gameplay_rules.world_multiplier = 2;
    globals.gameplay_rules.friendly_world_margin = 100.0F;

    StdRandomSource random(5U);
    SimWorld world(roster, globals, random);
    const SimSpawnResult mover = world.spawn("walker", UnitSide::FRIENDLY, {.x = 0.0F, .y = BELT_Y});

    for (int tick = 0; tick < 120; ++tick) {
        world.tick();
    }

    const SimEntity *walker_entity = world.find_entity(mover.id);
    DEFN_REQUIRE(walker_entity != nullptr);
    DEFN_CHECK_CLOSE(walker_entity->position.x, 1900.0, 0.001);
}

DEFN_TEST(sim_world_promotes_a_friendly_that_crosses_the_damage_threshold) {
    SimRoster roster;
    roster.add(make_unit("brute", UnitSide::FRIENDLY, 100, 10));
    roster.add(make_unit("sponge", UnitSide::HOSTILE, 10000, 0));

    GlobalUnitConfig globals = make_globals();
    globals.field_promotion = {.damage_threshold = 25, .damage_multiplier = 2.0, .attack_period_multiplier = 0.5, .health_multiplier = 3.0};

    StdRandomSource random(11U);
    SimWorld world(roster, globals, random);
    const SimSpawnResult brute = world.spawn("brute", UnitSide::FRIENDLY, {.x = 0.0F, .y = BELT_Y});
    world.spawn("sponge", UnitSide::HOSTILE, {.x = 50.0F, .y = BELT_Y});

    const SimEntity *brute_entity = world.find_entity(brute.id);
    DEFN_REQUIRE(brute_entity != nullptr);
    DEFN_CHECK(!brute_entity->field_promotion.is_promoted());

    for (int tick = 0; tick < 300; ++tick) {
        world.tick();
    }

    // Three blows of ten cross a threshold of twenty-five; the promotion then doubles damage and triples health.
    DEFN_CHECK(brute_entity->field_promotion.is_promoted());
    DEFN_CHECK_EQ(brute_entity->max_hp, 300);
    DEFN_CHECK_EQ(brute_entity->hp, 300);
    DEFN_CHECK_CLOSE(brute_entity->combat.melee_attack_period_seconds, 0.5, 0.0001);
    DEFN_CHECK(brute_entity->attacks_landed >= 4);
    DEFN_CHECK(brute_entity->damage_dealt > 10 * brute_entity->attacks_landed - 10);
}

DEFN_TEST(sim_world_never_promotes_a_hostile) {
    SimRoster roster;
    roster.add(make_unit("sponge", UnitSide::FRIENDLY, 10000, 0));
    roster.add(make_unit("raider", UnitSide::HOSTILE, 100, 10));

    GlobalUnitConfig globals = make_globals();
    globals.field_promotion = {.damage_threshold = 25, .damage_multiplier = 2.0, .attack_period_multiplier = 0.5, .health_multiplier = 3.0};

    StdRandomSource random(13U);
    SimWorld world(roster, globals, random);
    world.spawn("sponge", UnitSide::FRIENDLY, {.x = 0.0F, .y = BELT_Y});
    const SimSpawnResult raider = world.spawn("raider", UnitSide::HOSTILE, {.x = 50.0F, .y = BELT_Y});

    for (int tick = 0; tick < 300; ++tick) {
        world.tick();
    }

    const SimEntity *raider_entity = world.find_entity(raider.id);
    DEFN_REQUIRE(raider_entity != nullptr);
    DEFN_CHECK(raider_entity->damage_dealt > 25);
    DEFN_CHECK(!raider_entity->field_promotion.is_promoted());
    DEFN_CHECK_EQ(raider_entity->max_hp, 100);
}

DEFN_TEST(sim_world_leaves_a_unit_that_cannot_fight_alone) {
    SimRoster roster;
    roster.add(make_unit("bystander", UnitSide::FRIENDLY, 100, 0));
    roster.add(make_unit("raider", UnitSide::HOSTILE, 100, 10));

    StdRandomSource random(17U);
    SimWorld world(roster, make_globals(), random);
    const SimSpawnResult bystander = world.spawn("bystander", UnitSide::FRIENDLY, {.x = 0.0F, .y = BELT_Y});
    world.spawn("raider", UnitSide::HOSTILE, {.x = 50.0F, .y = BELT_Y});

    const SimEngagementReport report = run_engagement(world, 60.0);

    const SimEntity *bystander_entity = world.find_entity(bystander.id);
    DEFN_REQUIRE(bystander_entity != nullptr);
    DEFN_CHECK_EQ(bystander_entity->attacks_landed, 0);
    DEFN_CHECK(bystander_entity->dead);
    DEFN_REQUIRE(report.winner.has_value());
    DEFN_CHECK_EQ(static_cast<int>(*report.winner), static_cast<int>(UnitSide::HOSTILE));
}

// Risk one in the plan: get the animation clock wrong and ranged units either freeze forever or fire at the wrong
// cadence. The spawn frame, not the attack period, is what actually paces a projectile shooter.
DEFN_TEST(sim_world_paces_a_projectile_shooter_by_its_shoot_animation) {
    SimRoster roster;
    // Eight attacks a second on paper, but the shell is only released on frame 8 of a one-second animation.
    ProjectileAttackConfig projectile;
    projectile.speed_pixels_per_second = 100.0F;
    projectile.spawn_animation_frame = 8;
    projectile.impact_damage = 10;
    UnitConfig lobber = make_lobber("lobber", UnitSide::HOSTILE, projectile);
    lobber.ranged_attack_period_seconds = 0.125;
    roster.add(lobber);
    roster.add(make_dummy("dummy", UnitSide::FRIENDLY, 10000));

    StdRandomSource random(23U);
    SimWorld world(roster, make_globals(), random);
    world.spawn("dummy", UnitSide::FRIENDLY, {.x = 700.0F, .y = BELT_Y});
    const SimSpawnResult shooter = world.spawn("lobber", UnitSide::HOSTILE, {.x = 1000.0F, .y = BELT_Y});
    const SimEntity *lobber_entity = world.find_entity(shooter.id);
    DEFN_REQUIRE(lobber_entity != nullptr);

    for (int tick = 0; tick < 40; ++tick) { // 0.67s: five attack periods have come and gone
        world.tick();
    }

    // A pending shot freezes its shooter, so a ready cooldown buys nothing until the shell is away.
    DEFN_CHECK(lobber_entity->pending_projectile.active);
    DEFN_CHECK_EQ(lobber_entity->projectiles_launched, 0);

    for (int tick = 0; tick < 260; ++tick) { // out to five seconds
        world.tick();
    }

    // Five seconds of a shell every 0.8s, not every 0.125s: the animation sets the cadence.
    DEFN_CHECK(lobber_entity->projectiles_launched >= 5);
    DEFN_CHECK(lobber_entity->projectiles_launched <= 7);
}

DEFN_TEST(sim_world_flies_a_shell_before_it_lands) {
    SimRoster roster;
    ProjectileAttackConfig projectile;
    projectile.speed_pixels_per_second = 300.0F;
    projectile.spawn_animation_frame = 0; // released the instant the shot is decided
    projectile.impact_damage = 10;
    UnitConfig lobber = make_lobber("lobber", UnitSide::HOSTILE, projectile);
    lobber.ranged_attack_period_seconds = 10.0; // one shell for the whole run
    roster.add(lobber);
    roster.add(make_dummy("dummy", UnitSide::FRIENDLY, 10000));

    StdRandomSource random(29U);
    SimWorld world(roster, make_globals(), random);
    const SimSpawnResult dummy = world.spawn("dummy", UnitSide::FRIENDLY, {.x = 700.0F, .y = BELT_Y});
    world.spawn("lobber", UnitSide::HOSTILE, {.x = 1000.0F, .y = BELT_Y});

    world.tick(); // the shooter joins the process group
    world.tick(); // and commits its first shot
    DEFN_REQUIRE(static_cast<int>(world.get_projectiles().size()) == 1);

    const SimEntity *dummy_entity = world.find_entity(dummy.id);
    DEFN_REQUIRE(dummy_entity != nullptr);

    // 300 pixels at 300 pixels per second: a full second of flight, and nothing is hurt until it lands.
    for (int tick = 0; tick < 40; ++tick) {
        world.tick();
    }
    DEFN_CHECK_EQ(dummy_entity->hp, 10000);
    DEFN_REQUIRE(static_cast<int>(world.get_projectiles().size()) == 1);
    DEFN_CHECK(world.get_projectiles().front().flight.travelled_distance > 0.0F);

    for (int tick = 0; tick < 40; ++tick) {
        world.tick();
    }
    DEFN_CHECK_EQ(dummy_entity->hp, 9990);
    DEFN_CHECK_EQ(static_cast<int>(world.get_projectiles().size()), 0);
}

DEFN_TEST(sim_world_splashes_a_cluster_by_candidate_order_not_by_distance) {
    SimRoster roster;
    ProjectileAttackConfig projectile;
    projectile.speed_pixels_per_second = 1800.0F;
    projectile.splash_radius = 140.0F;
    projectile.affected_fraction = 0.5F;
    projectile.min_affected_targets = 1;
    projectile.affected_target_rounding = SplashTargetRoundingMode::NEAREST;
    projectile.include_direct_target = true;
    projectile.impact_damage = 10;
    projectile.splash_damage = 7;
    roster.add(make_lobber("lobber", UnitSide::HOSTILE, projectile));
    roster.add(make_dummy("dummy", UnitSide::FRIENDLY, 10000));

    StdRandomSource random(31U);
    SimWorld world(roster, make_globals(), random);
    const SimSpawnResult first = world.spawn("dummy", UnitSide::FRIENDLY, {.x = 700.0F, .y = BELT_Y});
    const SimSpawnResult second = world.spawn("dummy", UnitSide::FRIENDLY, {.x = 720.0F, .y = BELT_Y});
    const SimSpawnResult third = world.spawn("dummy", UnitSide::FRIENDLY, {.x = 740.0F, .y = BELT_Y});
    world.spawn("lobber", UnitSide::HOSTILE, {.x = 1000.0F, .y = BELT_Y});

    for (int tick = 0; tick < 30; ++tick) {
        world.tick();
    }

    // All three are inside the blast, but half of three rounds to two victims, and the shipped game picks them in the
    // order it walks the entity container -- spawn order -- rather than by how close they stand to the impact.
    DEFN_CHECK_EQ(world.find_entity(third.id)->hp, 9990);   // the direct target takes impact damage
    DEFN_CHECK_EQ(world.find_entity(first.id)->hp, 9993);   // first in spawn order takes splash
    DEFN_CHECK_EQ(world.find_entity(second.id)->hp, 10000); // nearer the blast, but trimmed from the list
}

DEFN_TEST(sim_world_lands_a_direct_hit_even_where_the_target_no_longer_is) {
    SimRoster roster;
    ProjectileAttackConfig projectile;
    projectile.speed_pixels_per_second = 60.0F; // slow enough that a walker outruns the aim point
    projectile.splash_radius = 10.0F;
    projectile.affected_fraction = 1.0F;
    projectile.impact_damage = 10;
    projectile.splash_damage = 7;
    roster.add(make_lobber("lobber", UnitSide::HOSTILE, projectile));

    UnitConfig runner = make_dummy("runner", UnitSide::FRIENDLY, 10000);
    runner.move_speed_pixels_per_second = 200.0F;
    runner.melee_damage = 1; // enough to give it a combat step, so it walks
    runner.melee_attack_range = 1.0F;
    roster.add(runner);

    StdRandomSource random(37U);
    SimWorld world(roster, make_globals(), random);
    const SimSpawnResult mover = world.spawn("runner", UnitSide::FRIENDLY, {.x = 300.0F, .y = BELT_Y});
    world.spawn("lobber", UnitSide::HOSTILE, {.x = 1000.0F, .y = BELT_Y});

    for (int tick = 0; tick < 240; ++tick) {
        world.tick();
    }

    // Aim is frozen at launch and the shell has no homing, so it detonates where the runner used to be. The direct
    // target is damaged anyway: the shipped impact rule applies impact damage by identity, not by proximity.
    const SimEntity *runner_entity = world.find_entity(mover.id);
    DEFN_REQUIRE(runner_entity != nullptr);
    DEFN_CHECK(runner_entity->damage_taken > 0);
    DEFN_CHECK_EQ(runner_entity->damage_taken % 10, 0);
}

namespace {

// One shell into three inert dummies. Spacing is the only variable, so the difference is the splash tax and nothing
// else: no approach to confound it, no return fire, no second shot.
int splash_damage_for_spacing(float spacing) {
    SimRoster roster;
    ProjectileAttackConfig projectile;
    projectile.speed_pixels_per_second = 1800.0F;
    projectile.splash_radius = 140.0F;
    projectile.affected_fraction = 0.5F;
    projectile.min_affected_targets = 1;
    projectile.affected_target_rounding = SplashTargetRoundingMode::NEAREST;
    projectile.include_direct_target = true;
    projectile.impact_damage = 10;
    projectile.splash_damage = 7;
    UnitConfig lobber = make_lobber("lobber", UnitSide::HOSTILE, projectile);
    lobber.ranged_attack_period_seconds = 10.0; // one shell for the whole run
    roster.add(lobber);
    roster.add(make_dummy("dummy", UnitSide::FRIENDLY, 10000));

    StdRandomSource random(41U);
    SimWorld world(roster, make_globals(), random);
    world.spawn("dummy", UnitSide::FRIENDLY, {.x = 740.0F - (2.0F * spacing), .y = BELT_Y});
    world.spawn("dummy", UnitSide::FRIENDLY, {.x = 740.0F - spacing, .y = BELT_Y});
    world.spawn("dummy", UnitSide::FRIENDLY, {.x = 740.0F, .y = BELT_Y});
    world.spawn("lobber", UnitSide::HOSTILE, {.x = 1000.0F, .y = BELT_Y});

    for (int tick = 0; tick < 40; ++tick) {
        world.tick();
    }

    return total_damage_taken(world, UnitSide::FRIENDLY);
}

} // namespace

// The splash rule itself, with spacing as the only variable: standing together costs a group 70% more per shell
// than standing apart. The lobber carries its own numbers rather than the mason's, so this stays a test of the
// rule -- including the `affected_fraction` below 1 that no shipped unit uses today.
DEFN_TEST(sim_world_measures_the_splash_tax_on_a_clustered_group) {
    const int clustered = splash_damage_for_spacing(20.0F);
    const int spread = splash_damage_for_spacing(320.0F);

    DEFN_CHECK_EQ(clustered, 17); // the direct hit for 10, plus one splash victim for 7
    DEFN_CHECK_EQ(spread, 10);    // nobody else is inside the blast, so only the direct target pays
}

DEFN_TEST(sim_world_replays_identically_from_the_same_seed) {
    const auto run = [](uint32_t seed) {
        SimRoster roster = make_shipped_roster();
        StdRandomSource random(seed);
        SimWorld world(roster, make_globals(), random);
        world.spawn("breacher", UnitSide::FRIENDLY, {.x = 1000.0F, .y = BELT_Y});
        world.spawn("grime", UnitSide::HOSTILE, {.x = 1400.0F, .y = BELT_Y});
        world.spawn("grime", UnitSide::HOSTILE, {.x = 1550.0F, .y = BELT_Y});
        return run_engagement(world, 60.0);
    };

    const SimEngagementReport first = run(4242U);
    const SimEngagementReport second = run(4242U);

    DEFN_CHECK_EQ(first.resolved, second.resolved);
    DEFN_CHECK_CLOSE(first.duration_seconds, second.duration_seconds, 0.0);
    DEFN_CHECK_EQ(first.friendly.hp_remaining, second.friendly.hp_remaining);
    DEFN_CHECK_EQ(first.friendly.damage_dealt, second.friendly.damage_dealt);
    DEFN_CHECK_EQ(first.hostile.damage_dealt, second.hostile.damage_dealt);
}

namespace {

// The lab question Phase 2 exists to answer: how does one roster entry actually fare against a known threat?
SimEngagementReport run_versus_three(const std::string &unit_id, const std::string &hostile_id) {
    SimRoster roster = make_shipped_roster();
    StdRandomSource random(2026U);
    SimWorld world(roster, make_globals(), random);

    world.spawn(unit_id, UnitSide::FRIENDLY, {.x = 1000.0F, .y = BELT_Y});
    world.spawn(hostile_id, UnitSide::HOSTILE, {.x = 1500.0F, .y = BELT_Y});
    world.spawn(hostile_id, UnitSide::HOSTILE, {.x = 1650.0F, .y = BELT_Y});
    world.spawn(hostile_id, UnitSide::HOSTILE, {.x = 1800.0F, .y = BELT_Y});

    return run_engagement(world, 60.0);
}

SimEngagementReport run_versus_three_grime(const std::string &unit_id) { return run_versus_three(unit_id, "grime"); }

} // namespace

// Measured, not assumed: the cheapest anchor is not an answer to three grime. It trades two for itself and leaves the
// third alive on most of its health. Pinning it here turns a balance fact into a regression test on the combat rules.
DEFN_TEST(sim_world_measures_breacher_losing_to_three_grime) {
    const SimEngagementReport report = run_versus_three_grime("breacher");

    DEFN_CHECK(report.resolved);
    DEFN_REQUIRE(report.winner.has_value());
    DEFN_CHECK_EQ(static_cast<int>(*report.winner), static_cast<int>(UnitSide::HOSTILE));
    DEFN_CHECK_EQ(report.friendly.alive, 0);
    DEFN_CHECK_EQ(report.hostile.alive, 1);
    DEFN_CHECK_EQ(report.hostile.damage_dealt, 400); // exactly the breacher's health pool
    DEFN_CHECK_CLOSE(report.duration_seconds, 24.9, 1.0);
}

// The long-range pick clears the same threat without moving a pixel: it engages at 650 and the grime stall at 345.
DEFN_TEST(sim_world_measures_marksman_clearing_three_grime) {
    const SimEngagementReport report = run_versus_three_grime("marksman");

    DEFN_CHECK(report.resolved);
    DEFN_REQUIRE(report.winner.has_value());
    DEFN_CHECK_EQ(static_cast<int>(*report.winner), static_cast<int>(UnitSide::FRIENDLY));
    DEFN_CHECK_EQ(report.friendly.alive, 1);
    DEFN_CHECK_EQ(report.hostile.alive, 0);
    DEFN_CHECK_EQ(report.friendly.damage_dealt, 285); // three grime, killed with overkill on the last blow of each
    DEFN_CHECK(report.friendly.hp_remaining > 0);
    DEFN_CHECK(report.friendly.hp_remaining < 90); // it survives, but on under half its health
    DEFN_CHECK_CLOSE(report.duration_seconds, 14.7, 1.0);
}

// The same breacher against the same count of a different threat. Masons kill it about 1.4 times faster than grime
// do, which is the threat ratio BALANCE.md already estimates -- now measured rather than guessed.
DEFN_TEST(sim_world_measures_breacher_losing_faster_to_three_mason_than_to_three_grime) {
    const SimEngagementReport versus_grime = run_versus_three("breacher", "grime");
    const SimEngagementReport versus_mason = run_versus_three("breacher", "mason");

    DEFN_REQUIRE(versus_grime.winner.has_value());
    DEFN_REQUIRE(versus_mason.winner.has_value());
    DEFN_CHECK_EQ(static_cast<int>(*versus_grime.winner), static_cast<int>(UnitSide::HOSTILE));
    DEFN_CHECK_EQ(static_cast<int>(*versus_mason.winner), static_cast<int>(UnitSide::HOSTILE));

    // Both kill the breacher outright, so the comparable number is how long it lasted and how much it took with it.
    DEFN_CHECK_CLOSE(versus_mason.duration_seconds, 17.7, 1.0);
    DEFN_CHECK(versus_mason.duration_seconds < versus_grime.duration_seconds);
    DEFN_CHECK_EQ(versus_grime.hostile.alive, 1); // the breacher trades itself for two grime
    DEFN_CHECK_EQ(versus_mason.hostile.alive, 2); // but only for one mason
}

DEFN_TEST(sim_world_reports_an_undecided_run_when_the_clock_runs_out) {
    SimRoster roster;
    roster.add(make_unit("bystander", UnitSide::FRIENDLY, 100, 0));
    roster.add(make_unit("post", UnitSide::HOSTILE, 100, 0));

    StdRandomSource random(19U);
    SimWorld world(roster, make_globals(), random);
    world.spawn("bystander", UnitSide::FRIENDLY, {.x = 0.0F, .y = BELT_Y});
    world.spawn("post", UnitSide::HOSTILE, {.x = 50.0F, .y = BELT_Y});

    const SimEngagementReport report = run_engagement(world, 2.0);

    DEFN_CHECK(!report.resolved);
    DEFN_CHECK(!report.winner.has_value());
    DEFN_CHECK_EQ(report.friendly.alive, 1);
    DEFN_CHECK_EQ(report.hostile.alive, 1);
}

} // namespace defn