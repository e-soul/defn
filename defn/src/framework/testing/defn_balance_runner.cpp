// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "defn_balance_runner.h"

#include "data_paths.h"
#include "godot_string.h"
#include "sim_roster.h"
#include "sim_world.h"
#include "unit_data.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <format>
#include <string>
#include <vector>

namespace defn {

namespace {

constexpr float BELT_Y = 800.0F;
constexpr double MAX_ENGAGEMENT_SECONDS = 180.0;

// The hostile roster, and the one every threat number is expressed relative to.
constexpr auto BASELINE_HOSTILE = "grime";
const std::vector<std::string> HOSTILES = {"grime", "mason", "wrecker", "jackal"};
const std::vector<std::string> FRIENDLIES = {"breacher", "marksman", "impact", "operator"};

struct EngagementOutcome {
    bool friendly_won = false;
    double duration_seconds = 0.0;
    int friendly_damage_taken = 0;
    int hostiles_killed = 0;
    int friendlies_lost = 0;
};

EngagementOutcome run_engagement_once(const SimRoster &roster, const GlobalUnitConfig &globals, const std::vector<std::string> &friendlies,
                                      const std::vector<std::string> &hostiles, std::uint32_t seed) {
    StdRandomSource random(seed);
    SimWorld world(roster, globals, random);

    float friendly_x = 800.0F;
    for (const std::string &unit_id : friendlies) {
        world.spawn(unit_id, UnitSide::FRIENDLY, {.x = friendly_x, .y = BELT_Y});
        friendly_x -= 70.0F;
    }

    float hostile_x = 1600.0F;
    for (const std::string &unit_id : hostiles) {
        world.spawn(unit_id, UnitSide::HOSTILE, {.x = hostile_x, .y = BELT_Y});
        hostile_x += 110.0F;
    }
    world.begin_run();

    const SimEngagementReport report = run_engagement(world, MAX_ENGAGEMENT_SECONDS);

    EngagementOutcome outcome;
    outcome.friendly_won = report.winner.has_value() && *report.winner == UnitSide::FRIENDLY;
    outcome.duration_seconds = report.duration_seconds;
    for (const SimEntity &entity : world.get_entities()) {
        if (entity.side == UnitSide::FRIENDLY) {
            outcome.friendly_damage_taken += entity.damage_taken;
            outcome.friendlies_lost += entity.dead ? 1 : 0;
        } else if (entity.dead) {
            ++outcome.hostiles_killed;
        }
    }

    return outcome;
}

struct Averaged {
    double win_rate = 0.0;
    double duration_seconds = 0.0;
    double friendly_damage_taken = 0.0;
    double hostiles_killed = 0.0;
    double friendlies_lost = 0.0;
};

Averaged average_engagement(const SimRoster &roster, const GlobalUnitConfig &globals, const std::vector<std::string> &friendlies,
                            const std::vector<std::string> &hostiles, int seeds) {
    Averaged total;
    for (int seed = 0; seed < seeds; ++seed) {
        const EngagementOutcome outcome = run_engagement_once(roster, globals, friendlies, hostiles, static_cast<std::uint32_t>(2026 + seed));
        total.win_rate += outcome.friendly_won ? 1.0 : 0.0;
        total.duration_seconds += outcome.duration_seconds;
        total.friendly_damage_taken += outcome.friendly_damage_taken;
        total.hostiles_killed += outcome.hostiles_killed;
        total.friendlies_lost += outcome.friendlies_lost;
    }

    const double count = std::max(seeds, 1);
    total.win_rate /= count;
    total.duration_seconds /= count;
    total.friendly_damage_taken /= count;
    total.hostiles_killed /= count;
    total.friendlies_lost /= count;
    return total;
}

std::vector<std::string> repeat(const std::string &unit_id, int count) { return std::vector<std::string>(static_cast<std::size_t>(count), unit_id); }

} // namespace

void DefnBalanceRunner::_bind_methods() { ClassDB::bind_static_method(get_class_static(), D_METHOD("measure", "args"), &DefnBalanceRunner::measure); }

Dictionary DefnBalanceRunner::measure(const Dictionary &args) {
    const int seeds = std::max(static_cast<int>(static_cast<int64_t>(args.get("seeds", 25))), 1);
    const String out_path = args.get("out", String());

    UnitDataLoader loader;
    if (!loader.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS)) {
        UtilityFunctions::printerr("DefnBalanceRunner: could not load unit data.");
        Dictionary failure;
        failure["success"] = false;
        return failure;
    }

    SimRoster roster;
    for (const std::string &unit_id : HOSTILES) {
        if (const auto config = loader.get_unit(unit_id); config.has_value()) {
            roster.add(*config);
        }
    }
    for (const std::string &unit_id : FRIENDLIES) {
        if (const auto config = loader.get_unit(unit_id); config.has_value()) {
            roster.add(*config);
        }
    }

    const GlobalUnitConfig &globals = loader.get_globals();
    String report;

    // Threat: what the player pays per kill, which is what a threat point is trying to express. The reference has to
    // beat every hostile type and still bleed doing it -- a marksman wall out-ranges the whole roster and takes zero
    // damage, which measures nothing. Six breachers are out-ranged by everything, so they have to walk in and trade.
    const std::vector<std::string> reference_defence = repeat("breacher", 6);
    Array threat_rows;
    double baseline_cost = 0.0;
    for (const std::string &unit_id : HOSTILES) {
        const Averaged result = average_engagement(roster, globals, reference_defence, repeat(unit_id, 4), seeds);
        const double cost_per_kill = result.hostiles_killed > 0.0 ? result.friendly_damage_taken / result.hostiles_killed : 0.0;
        if (unit_id == BASELINE_HOSTILE) {
            baseline_cost = cost_per_kill;
        }

        Dictionary row;
        row["unit_id"] = to_godot_string(unit_id);
        row["win_rate"] = result.win_rate;
        row["seconds"] = result.duration_seconds;
        row["cost_per_kill"] = cost_per_kill;
        row["defenders_lost"] = result.friendlies_lost;
        threat_rows.push_back(row);
    }

    report += "threat: six breachers against four of each hostile, averaged over seeds\n";
    report += "hostile   win%   secs   hp/kill  threat  lost\n";
    for (const Variant &value : threat_rows) {
        Dictionary row = value;
        const double cost = row["cost_per_kill"];
        const double threat = baseline_cost > 0.0 ? cost / baseline_cost : 0.0;
        row["threat"] = threat;
        report += to_godot_string(std::format("{:<9} {:>4.0f}  {:>6.1f}  {:>7.1f}  {:>6.2f}  {:>4.1f}\n", to_std_string(row["unit_id"]),
                                              100.0 * static_cast<double>(row["win_rate"]), static_cast<double>(row["seconds"]), cost, threat,
                                              static_cast<double>(row["defenders_lost"])));
    }

    // Roster: what a fixed energy budget buys. Counts differ so that spend does not -- otherwise the comparison is
    // between three cheap units and three expensive ones, which says nothing about whether either is worth its cost.
    constexpr int ROSTER_ENERGY_BUDGET = 120;
    const std::vector<std::string> reference_threat = repeat(BASELINE_HOSTILE, 8);
    Array roster_rows;
    report += to_godot_string(std::format("\nroster: {} energy of each friendly against eight grime\n", ROSTER_ENERGY_BUDGET));
    report += "friendly  cost  bought  spent  win%   secs   hp lost  lost\n";
    for (const std::string &unit_id : FRIENDLIES) {
        const auto config = loader.get_unit(unit_id);
        const int unit_cost = config.has_value() ? config->cost : 0;
        const int bought = unit_cost > 0 ? std::max(ROSTER_ENERGY_BUDGET / unit_cost, 1) : 1;
        const Averaged result = average_engagement(roster, globals, repeat(unit_id, bought), reference_threat, seeds);

        Dictionary row;
        row["unit_id"] = to_godot_string(unit_id);
        row["cost"] = unit_cost;
        row["bought"] = bought;
        row["spent"] = bought * unit_cost;
        row["win_rate"] = result.win_rate;
        row["seconds"] = result.duration_seconds;
        row["hp_lost"] = result.friendly_damage_taken;
        row["units_lost"] = result.friendlies_lost;
        roster_rows.push_back(row);

        report +=
            to_godot_string(std::format("{:<9} {:>4}  {:>6}  {:>5}  {:>4.0f}  {:>6.1f}  {:>7.0f}  {:>4.1f}\n", unit_id, unit_cost, bought, bought * unit_cost,
                                        100.0 * result.win_rate, result.duration_seconds, result.friendly_damage_taken, result.friendlies_lost));
    }

    UtilityFunctions::print(report);
    if (!out_path.is_empty()) {
        Ref<FileAccess> file = FileAccess::open(out_path, FileAccess::WRITE);
        if (file.is_valid()) {
            file->store_string(report);
            file->close();
        }
    }

    Dictionary result;
    result["success"] = true;
    result["seeds"] = seeds;
    result["threat"] = threat_rows;
    result["roster"] = roster_rows;
    return result;
}

} // namespace defn
