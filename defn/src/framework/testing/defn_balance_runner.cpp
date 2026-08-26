// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "defn_balance_runner.h"

#include "data_paths.h"
#include "godot_string.h"
#include "sim_engagement_lab.h"
#include "sim_roster.h"
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

// The hostile roster, and the one every threat number is expressed relative to. Both lists are defaults: a caller
// that wants a different slice of the catalog passes its own.
constexpr auto BASELINE_HOSTILE = "grime";
const std::vector<std::string> DEFAULT_HOSTILES = {"grime", "mason", "wrecker", "jackal", "hound"};
const std::vector<std::string> DEFAULT_FRIENDLIES = {"breacher", "marksman", "impact", "operator"};

std::vector<std::string> to_id_list(const Variant &value, const std::vector<std::string> &fallback) {
    const Array entries = value;
    if (entries.is_empty()) {
        return fallback;
    }

    std::vector<std::string> ids;
    ids.reserve(entries.size());
    for (const Variant &entry : entries) {
        ids.push_back(to_std_string(entry));
    }
    return ids;
}

} // namespace

void DefnBalanceRunner::_bind_methods() { ClassDB::bind_static_method(get_class_static(), D_METHOD("measure", "args"), &DefnBalanceRunner::measure); }

Dictionary DefnBalanceRunner::measure(const Dictionary &args) {
    const int seed_count = std::max(static_cast<int>(static_cast<int64_t>(args.get("seeds", 25))), 1);
    const String out_path = args.get("out", String());
    const std::vector<std::string> hostiles = to_id_list(args.get("hostiles", Array()), DEFAULT_HOSTILES);
    const std::vector<std::string> friendlies = to_id_list(args.get("friendlies", Array()), DEFAULT_FRIENDLIES);

    UnitDataLoader loader;
    if (!loader.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS)) {
        UtilityFunctions::printerr("DefnBalanceRunner: could not load unit data.");
        Dictionary failure;
        failure["success"] = false;
        return failure;
    }

    SimRoster roster;
    for (const std::string &unit_id : hostiles) {
        if (const auto config = loader.get_unit(unit_id); config.has_value()) {
            roster.add(*config);
        }
    }
    for (const std::string &unit_id : friendlies) {
        if (const auto config = loader.get_unit(unit_id); config.has_value()) {
            roster.add(*config);
        }
    }

    const GlobalUnitConfig &globals = loader.get_globals();
    const std::vector<std::uint32_t> seeds = default_seeds(seed_count);
    String report;

    // Threat: what the player pays per kill, which is what a threat point is trying to express. The reference has to
    // beat every hostile type and still bleed doing it -- a marksman wall out-ranges the whole roster and takes zero
    // damage, which measures nothing. Six breachers are out-ranged by everything, so they have to walk in and trade.
    const ForceMix reference_defence = mono_mix("breacher", 6);
    Array threat_rows;
    double baseline_cost = 0.0;
    for (const std::string &unit_id : hostiles) {
        const AveragedEngagement result = average_engagement(roster, globals, reference_defence, mono_mix(unit_id, 4), seeds);
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
    const ForceMix reference_threat = mono_mix(BASELINE_HOSTILE, 8);
    Array roster_rows;
    report += to_godot_string(std::format("\nroster: {} energy of each friendly against eight grime\n", ROSTER_ENERGY_BUDGET));
    report += "friendly  cost  bought  spent  win%   secs   hp lost  lost\n";
    for (const std::string &unit_id : friendlies) {
        const auto config = loader.get_unit(unit_id);
        const int unit_cost = config.has_value() ? config->cost : 0;
        const int bought = unit_cost > 0 ? std::max(ROSTER_ENERGY_BUDGET / unit_cost, 1) : 1;
        const AveragedEngagement result = average_engagement(roster, globals, mono_mix(unit_id, bought), reference_threat, seeds);

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
    result["seeds"] = seed_count;
    result["threat"] = threat_rows;
    result["roster"] = roster_rows;
    return result;
}

} // namespace defn
