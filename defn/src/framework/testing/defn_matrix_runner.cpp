// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "defn_matrix_runner.h"

#include "data_paths.h"
#include "godot_string.h"
#include "json_file_loader.h"
#include "sim_engagement_lab.h"
#include "sim_roster.h"
#include "unit_data.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <format>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace defn {

namespace {

const std::vector<std::string> DEFAULT_FRIENDLIES = {"breacher", "marksman", "impact", "operator"};
const std::vector<std::string> DEFAULT_HOSTILES = {"grime", "mason", "wrecker", "jackal", "hound"};

// Six a side for a hostile column, split evenly when the column is a pair. The absolute number only sets the scale of
// every `B*`; what matters is that it is the same for every column, so the columns stay comparable.
constexpr int HOSTILE_FORCE_SIZE = 6;
constexpr int LABEL_WIDTH = 22;
constexpr int CELL_WIDTH = 10;

struct FriendlyMix {
    std::string label;
    MixShape shape;
};

struct HostileMix {
    std::string label;
    ForceMix force;
};

struct MatrixPlan {
    std::vector<FriendlyMix> friendly_mixes;
    std::vector<HostileMix> hostile_mixes;
    // Fifteen rather than five. At five the 2-sigma floor was 13% of budget, against a decision-regret band that
    // starts at 10% -- the instrument could not resolve the number it was being asked to judge. Cost is linear in
    // seeds and the error falls with its square root, so this buys roughly a 7-8% floor.
    CriticalBudgetOptions options;
    int seed_count = 15;
    // How far apart the two lines start. Exposed because a longer walk is not just a delay: friendlies advance at
    // their own speeds, so a mixed force with a wide speed spread arrives strung out rather than as a line, and the
    // fast half meets the enemy alone. A set-piece at one fixed separation cannot see that at all.
    LabSetup setup;
};

struct MatrixResult {
    String lines;
    String table;
    int rows = 0;
};

std::string join_labels(const std::vector<std::string> &parts) {
    std::string label;
    for (const std::string &part : parts) {
        if (!label.empty()) {
            label += '+';
        }
        label += part;
    }
    return label;
}

// Monos first, then every unordered pair at even weight. Monos give the transitive axis `a[i]`; the pairs are the only
// rows that can carry an interaction, so a matrix of monos alone would answer the diversity question "no" by
// construction.
std::vector<FriendlyMix> default_friendly_mixes(const std::vector<std::string> &friendlies) {
    const std::size_t count = friendlies.size();
    std::vector<FriendlyMix> mixes;
    mixes.reserve(count + (count * (count - 1) / 2));
    for (const std::string &unit_id : friendlies) {
        mixes.push_back({.label = unit_id, .shape = {{.unit_id = unit_id, .weight = 1.0}}});
    }
    for (std::size_t left = 0; left < count; ++left) {
        for (std::size_t right = left + 1; right < count; ++right) {
            mixes.push_back({.label = join_labels({friendlies[left], friendlies[right]}),
                             .shape = {{.unit_id = friendlies[left], .weight = 1.0}, {.unit_id = friendlies[right], .weight = 1.0}}});
        }
    }
    return mixes;
}

std::vector<HostileMix> default_hostile_mixes(const std::vector<std::string> &hostiles) {
    const std::size_t count = hostiles.size();
    std::vector<HostileMix> mixes;
    mixes.reserve(count + (count * (count - 1) / 2));
    for (const std::string &unit_id : hostiles) {
        mixes.push_back({.label = unit_id, .force = mono_mix(unit_id, HOSTILE_FORCE_SIZE)});
    }
    for (std::size_t left = 0; left < count; ++left) {
        for (std::size_t right = left + 1; right < count; ++right) {
            mixes.push_back(
                {.label = join_labels({hostiles[left], hostiles[right]}),
                 .force = {{.unit_id = hostiles[left], .count = HOSTILE_FORCE_SIZE / 2}, {.unit_id = hostiles[right], .count = HOSTILE_FORCE_SIZE / 2}}});
        }
    }
    return mixes;
}

// `{"breacher": 2, "marksman": 1}` reads back as `breacher2+marksman1`, so a row label still carries the ratio when
// the spec did not bother to name the mix. A weight of one is left implicit.
std::string default_label(const Dictionary &entries) {
    const Array keys = entries.keys();
    std::vector<std::string> parts;
    parts.reserve(keys.size());
    for (const Variant &key : keys) {
        const auto amount = static_cast<double>(entries[key]);
        parts.push_back(to_std_string(key) + (amount == 1.0 ? "" : std::format("{:g}", amount)));
    }
    return join_labels(parts);
}

std::vector<FriendlyMix> parse_friendly_mixes(const Array &entries) {
    std::vector<FriendlyMix> mixes;
    mixes.reserve(entries.size());
    for (const Variant &value : entries) {
        const Dictionary entry = value;
        const Dictionary weights = entry.get("weights", Dictionary());
        const Array unit_ids = weights.keys();
        FriendlyMix mix;
        mix.label = to_std_string(entry.get("label", to_godot_string(default_label(weights))));
        mix.shape.reserve(unit_ids.size());
        for (const Variant &key : unit_ids) {
            mix.shape.push_back({.unit_id = to_std_string(key), .weight = static_cast<double>(weights[key])});
        }
        mixes.push_back(mix);
    }
    return mixes;
}

std::vector<HostileMix> parse_hostile_mixes(const Array &entries) {
    std::vector<HostileMix> mixes;
    mixes.reserve(entries.size());
    for (const Variant &value : entries) {
        const Dictionary entry = value;
        const Dictionary units = entry.get("units", Dictionary());
        const Array unit_ids = units.keys();
        HostileMix mix;
        mix.label = to_std_string(entry.get("label", to_godot_string(default_label(units))));
        mix.force.reserve(unit_ids.size());
        for (const Variant &key : unit_ids) {
            mix.force.push_back({.unit_id = to_std_string(key), .count = static_cast<int>(static_cast<int64_t>(units[key]))});
        }
        mixes.push_back(mix);
    }
    return mixes;
}

MatrixPlan plan_matrix(const Dictionary &spec, const Dictionary &args) {
    MatrixPlan plan;
    plan.seed_count = std::max(static_cast<int>(static_cast<int64_t>(args.get("seeds", spec.get("seeds", plan.seed_count)))), 1);
    plan.options.max_budget = static_cast<double>(spec.get("max_budget", plan.options.max_budget));
    plan.options.tolerance = static_cast<double>(spec.get("tolerance", plan.options.tolerance));
    plan.options.max_iterations = static_cast<int>(static_cast<int64_t>(spec.get("max_iterations", plan.options.max_iterations)));
    plan.options.win_threshold = static_cast<double>(spec.get("win_threshold", plan.options.win_threshold));

    const double default_separation = plan.setup.hostile_front_x - plan.setup.friendly_front_x;
    const auto separation = static_cast<float>(static_cast<double>(spec.get("separation", args.get("separation", default_separation))));
    plan.setup.hostile_front_x = plan.setup.friendly_front_x + separation;

    // How far apart the friendly line stands. Load-bearing for anything melee: the default 70px is *inside* a 128px
    // melee reach, so a unit that means to walk past the front rank finds the back rank already in contact and has
    // nothing to walk past. A lab that cannot separate a front line from a backline cannot measure a dive.
    const auto friendly_spacing =
        static_cast<float>(static_cast<double>(spec.get("friendly_spacing", args.get("friendly_spacing", plan.setup.friendly_spacing))));
    if (friendly_spacing > 0.0F) {
        plan.setup.friendly_spacing = friendly_spacing;
    }

    plan.friendly_mixes = parse_friendly_mixes(spec.get("friendly_mixes", Array()));
    if (plan.friendly_mixes.empty()) {
        plan.friendly_mixes = default_friendly_mixes(DEFAULT_FRIENDLIES);
    }
    plan.hostile_mixes = parse_hostile_mixes(spec.get("hostile_mixes", Array()));
    if (plan.hostile_mixes.empty()) {
        plan.hostile_mixes = default_hostile_mixes(DEFAULT_HOSTILES);
    }
    return plan;
}

// Whatever the mixes name, and nothing else: the roster is what the lab is allowed to spawn.
std::set<std::string> referenced_units(const MatrixPlan &plan) {
    std::set<std::string> referenced;
    for (const FriendlyMix &mix : plan.friendly_mixes) {
        for (const MixWeight &entry : mix.shape) {
            referenced.insert(entry.unit_id);
        }
    }
    for (const HostileMix &mix : plan.hostile_mixes) {
        for (const MixEntry &entry : mix.force) {
            referenced.insert(entry.unit_id);
        }
    }
    return referenced;
}

Dictionary to_dictionary(const MixShape &shape) {
    Dictionary weights;
    for (const MixWeight &entry : shape) {
        weights[to_godot_string(entry.unit_id)] = entry.weight;
    }
    return weights;
}

Dictionary to_dictionary(const ForceMix &mix) {
    Dictionary counts;
    for (const MixEntry &entry : mix) {
        counts[to_godot_string(entry.unit_id)] = entry.count;
    }
    return counts;
}

Dictionary make_row(const FriendlyMix &friendly, const HostileMix &hostile, std::uint32_t seed, const CriticalBudget &budget, double max_budget,
                    float separation) {
    Dictionary row;
    row["friendly_mix"] = to_godot_string(friendly.label);
    row["friendly_weights"] = to_dictionary(friendly.shape);
    row["hostile_mix"] = to_godot_string(hostile.label);
    row["hostile_units"] = to_dictionary(hostile.force);
    row["seed"] = static_cast<int64_t>(seed);
    row["bounded"] = budget.bounded;
    row["critical_budget"] = budget.energy;
    row["max_budget"] = max_budget;
    row["separation"] = separation;
    row["bought"] = to_dictionary(budget.bought);
    row["energy_spent"] = budget.energy_spent;
    row["win_rate"] = budget.win_rate;
    row["duration_seconds"] = budget.duration_seconds;
    row["hostiles_killed"] = budget.hostiles_killed;
    row["friendly_damage_taken"] = budget.friendly_damage_taken;
    row["friendlies_lost"] = budget.friendlies_lost;
    row["probes"] = budget.probes;
    return row;
}

// One cell: every seed bisected on its own, so the spread across seeds is a real confidence interval rather than an
// average that has already thrown the noise away.
struct Cell {
    double bounded_total = 0.0;
    int bounded_count = 0;
};

Cell measure_cell(const UnitCatalog &catalog, const GlobalUnitConfig &globals, const FriendlyMix &friendly, const HostileMix &hostile,
                  std::span<const std::uint32_t> seeds, const MatrixPlan &plan, MatrixResult &into) {
    const float separation = plan.setup.hostile_front_x - plan.setup.friendly_front_x;
    Cell cell;
    for (const std::uint32_t seed : seeds) {
        const std::span<const std::uint32_t> single(&seed, 1);
        const CriticalBudget budget = critical_budget(catalog, globals, friendly.shape, hostile.force, single, plan.options, plan.setup);
        into.lines += JSON::stringify(make_row(friendly, hostile, seed, budget, plan.options.max_budget, separation)) + "\n";
        ++into.rows;
        if (budget.bounded) {
            cell.bounded_total += budget.energy;
            ++cell.bounded_count;
        }
    }
    return cell;
}

String format_cell(const Cell &cell, int seed_count) {
    if (cell.bounded_count == 0) {
        return to_godot_string(std::format(" {:>{}}", "unbounded", CELL_WIDTH));
    }
    if (cell.bounded_count < seed_count) {
        // Some seeds won and some never did. An average over only the winners would read as a budget that works.
        return to_godot_string(std::format(" {:>{}}", "partial", CELL_WIDTH));
    }
    return to_godot_string(std::format(" {:>{}.0f}", cell.bounded_total / static_cast<double>(cell.bounded_count), CELL_WIDTH));
}

MatrixResult run_matrix(const UnitCatalog &catalog, const GlobalUnitConfig &globals, const MatrixPlan &plan) {
    const std::vector<std::uint32_t> seeds = default_seeds(plan.seed_count);

    MatrixResult result;
    result.table = to_godot_string(std::format("matrix: critical budget B*, {} friendly mixes x {} hostile mixes, {} seed(s), separation {:.0f}px\n",
                                               plan.friendly_mixes.size(), plan.hostile_mixes.size(), plan.seed_count,
                                               plan.setup.hostile_front_x - plan.setup.friendly_front_x));
    result.table += to_godot_string(std::format("{:<{}}", "friendly \\ hostile", LABEL_WIDTH));
    for (const HostileMix &hostile : plan.hostile_mixes) {
        result.table += to_godot_string(std::format(" {:>{}}", hostile.label.substr(0, CELL_WIDTH), CELL_WIDTH));
    }
    result.table += "\n";

    for (const FriendlyMix &friendly : plan.friendly_mixes) {
        result.table += to_godot_string(std::format("{:<{}}", friendly.label.substr(0, LABEL_WIDTH), LABEL_WIDTH));
        for (const HostileMix &hostile : plan.hostile_mixes) {
            result.table += format_cell(measure_cell(catalog, globals, friendly, hostile, seeds, plan, result), plan.seed_count);
        }
        result.table += "\n";
    }
    return result;
}

Dictionary make_failure(const String &message) {
    UtilityFunctions::printerr(message);
    Dictionary result;
    result["success"] = false;
    result["message"] = message;
    return result;
}

} // namespace

void DefnMatrixRunner::_bind_methods() { ClassDB::bind_static_method(get_class_static(), D_METHOD("measure", "args"), &DefnMatrixRunner::measure); }

Dictionary DefnMatrixRunner::measure(const Dictionary &args) {
    const String spec_path = args.get("spec", String());
    const String out_path = args.get("out", String());

    Dictionary spec;
    if (!spec_path.is_empty()) {
        const auto loaded = JsonFileLoader::load_dictionary(spec_path, "DefnMatrixRunner");
        if (!loaded) {
            return make_failure(String("DefnMatrixRunner: could not read matrix spec: ") + spec_path);
        }
        spec = *loaded;
    }

    UnitDataLoader loader;
    if (!loader.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS)) {
        return make_failure("DefnMatrixRunner: could not load unit data.");
    }

    const MatrixPlan plan = plan_matrix(spec, args);
    SimRoster roster;
    for (const std::string &unit_id : referenced_units(plan)) {
        const auto config = loader.get_unit(unit_id);
        if (!config.has_value()) {
            return make_failure(String("DefnMatrixRunner: the matrix names a unit that is not in the catalog: ") + to_godot_string(unit_id));
        }
        roster.add(*config);
    }

    const MatrixResult measured = run_matrix(roster, loader.get_globals(), plan);
    UtilityFunctions::print(measured.table);
    if (out_path.is_empty()) {
        UtilityFunctions::print(measured.lines);
    } else {
        Ref<FileAccess> file = FileAccess::open(out_path, FileAccess::WRITE);
        if (!file.is_valid()) {
            return make_failure(String("DefnMatrixRunner: could not write: ") + out_path);
        }
        file->store_string(measured.lines);
        file->close();
    }

    Dictionary result;
    result["success"] = true;
    result["cells"] = static_cast<int64_t>(plan.friendly_mixes.size() * plan.hostile_mixes.size());
    result["rows"] = measured.rows;
    result["seeds"] = plan.seed_count;
    result["out"] = out_path;
    return result;
}

} // namespace defn
