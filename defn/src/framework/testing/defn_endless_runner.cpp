// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "defn_endless_runner.h"

#include "data_paths.h"
#include "endless_schedule_loader.h"
#include "godot_string.h"
#include "sim_match.h"
#include "sim_scenario.h"
#include "unit_data.h"
#include "upgrade_catalog.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <format>
#include <string>
#include <vector>

namespace defn {

namespace {

// Long enough for the run to be decided by the escalation rather than by the clock. A competent policy should die
// well inside this; a policy that does not is telling you the ramp is too gentle.
constexpr double DEFAULT_MAX_SECONDS = 1800.0;

// Cost grows with the waves a run reaches -- entity count climbs on both sides, and target selection is quadratic in
// what stands on the field -- but modestly: a full 53-wave run of the strongest composition is about seven seconds,
// and a thirty-run sweep under a minute. `peak_enemies` on every row reads that driver directly, and it sits in the
// tens rather than the hundreds.
//
// So a sweep that appears to hang is not the simulation being slow. It has, more than once, been an infinite loop in
// the runner script's argument parsing, which pegs a core, allocates nothing and writes no output -- from outside
// identical to a long sweep, except that it never ends. Check the parser before theorising about cost.

Dictionary make_failure(const String &message) {
    UtilityFunctions::printerr(message);
    Dictionary result;
    result["success"] = false;
    result["message"] = message;
    return result;
}

std::vector<double> parse_doubles(const Variant &value, double fallback) {
    const Array entries = value;
    if (entries.is_empty()) {
        return {fallback};
    }

    std::vector<double> values;
    values.reserve(entries.size());
    for (const Variant &entry : entries) {
        values.push_back(static_cast<double>(entry));
    }
    return values;
}

SimPolicySpec parse_policy(const Dictionary &policy) {
    SimPolicySpec spec;
    spec.kind = to_std_string(policy.get("kind", String("greedy")));
    spec.energy_reserve = static_cast<int>(static_cast<int64_t>(policy.get("energy_reserve", 15)));
    spec.label = to_std_string(String(policy.get("label", String())));

    const Dictionary weights = policy.get("weights", Dictionary());
    for (const Variant &unit_id : Array(weights.keys())) {
        spec.weights.emplace(to_std_string(unit_id), static_cast<double>(weights[unit_id]));
    }
    return spec;
}

// The default slate. The four standard policies say how long a run lasts; the mono mixes are the not-solved gate,
// and there has to be one per friendly unit or the gate cannot see the mix that solves the mode.
std::vector<SimPolicySpec> default_policies(const std::vector<std::string> &friendly_unit_ids) {
    std::vector<SimPolicySpec> policies = {
        {.kind = "greedy", .label = "greedy"},
        {.kind = "defensive", .label = "defensive"},
        {.kind = "patience", .label = "patience"},
    };

    for (const std::string &unit_id : friendly_unit_ids) {
        policies.push_back({.kind = "mix", .weights = {{unit_id, 1.0}}, .label = "mono:" + unit_id});
    }

    // Every unordered pair as well, so the gate is read over compositions and not only over single units.
    for (std::size_t first = 0; first < friendly_unit_ids.size(); ++first) {
        for (std::size_t second = first + 1; second < friendly_unit_ids.size(); ++second) {
            policies.push_back({.kind = "mix",
                                .weights = {{friendly_unit_ids[first], 1.0}, {friendly_unit_ids[second], 1.0}},
                                .label = "mix:" + friendly_unit_ids[first] + "+" + friendly_unit_ids[second]});
        }
    }
    return policies;
}

std::vector<SimPolicySpec> parse_policies(const Variant &value, const std::vector<std::string> &friendly_unit_ids) {
    const Array entries = value;
    if (entries.is_empty()) {
        return default_policies(friendly_unit_ids);
    }

    std::vector<SimPolicySpec> policies;
    policies.reserve(entries.size());
    for (const Variant &entry : entries) {
        policies.push_back(parse_policy(entry));
    }
    return policies;
}

// Narrows the default slate to the labels named, so a sweep over the knobs can be run without paying for every
// composition on every cell.
std::vector<SimPolicySpec> filter_by_label(std::vector<SimPolicySpec> policies, const Array &labels) {
    if (labels.is_empty()) {
        return policies;
    }

    std::vector<std::string> wanted;
    wanted.reserve(labels.size());
    for (const Variant &label : labels) {
        wanted.push_back(to_std_string(label));
    }

    std::erase_if(policies, [&wanted](const SimPolicySpec &policy) { return std::ranges::find(wanted, policy.label) == wanted.end(); });
    return policies;
}

// Every upgrade that unlocks a unit. A player reaching endless has cleared the whole campaign, so the sweep gives
// the run the roster that campaign opened -- otherwise every policy is the same policy, since only the base unit is
// deployable and the not-solved gate has nothing to see.
std::vector<std::string> roster_unlock_upgrades(const std::vector<ProgressionUpgradeCard> &cards) {
    std::vector<std::string> owned;
    for (const ProgressionUpgradeCard &card : cards) {
        const bool unlocks_a_unit =
            std::ranges::any_of(card.effects, [](const ProgressionUpgradeEffect &effect) { return effect.type == ProgressionUpgradeEffectType::UNIT_UNLOCK; });
        if (unlocks_a_unit) {
            owned.push_back(card.id);
        }
    }
    return owned;
}

std::vector<std::string> deployable_friendly_ids(const UnitDataLoader &catalog) {
    std::vector<std::string> ids;
    for (const UnitConfig &unit : catalog.get_friendly_units()) {
        if (unit.name != "base" && unit.cost > 0) {
            ids.push_back(unit.name);
        }
    }
    std::ranges::sort(ids);
    return ids;
}

// One cell of the sweep grid. Knobs are expanded into a flat list up front so `run_sweep` walks a single loop: one
// nested loop per knob puts the run body six levels deep, which is unreadable well before clang-tidy objects to it,
// and every knob added made it worse.
struct KnobSet {
    double base_budget = 0.0;
    double escalation = 0.0;
    double escalation_curve = 0.0;
    double bounty_decay = 0.0;
    double hostile_damage_growth = 0.0;
    double hostile_damage_cap = 0.0;
};

std::vector<KnobSet> expand_knobs(const std::vector<double> &base_budgets, const std::vector<double> &escalations, const std::vector<double> &curves,
                                  const std::vector<double> &decays, const std::vector<double> &growths, const std::vector<double> &caps) {
    std::vector<KnobSet> cells;
    cells.reserve(base_budgets.size() * escalations.size() * curves.size() * decays.size() * growths.size() * caps.size());
    for (const double base_budget : base_budgets) {
        for (const double escalation : escalations) {
            for (const double curve : curves) {
                for (const double decay : decays) {
                    for (const double growth : growths) {
                        for (const double cap : caps) {
                            cells.push_back({.base_budget = base_budget,
                                             .escalation = escalation,
                                             .escalation_curve = curve,
                                             .bounty_decay = decay,
                                             .hostile_damage_growth = growth,
                                             .hostile_damage_cap = cap});
                        }
                    }
                }
            }
        }
    }
    return cells;
}

// Per-unit spawns and deaths. Whether the player's line is *churning* -- taking losses and buying replacements -- or
// simply standing there is what every attrition-shaped lever depends on: starving income can only shrink an army
// that needs replacing.
std::string deaths_trace(const std::vector<SimUnitStat> &per_unit) {
    std::string trace = "{";
    bool first = true;
    for (const SimUnitStat &stat : per_unit) {
        trace += (first ? "" : ",") + std::format(R"("{}":[{},{}])", stat.unit_id, stat.spawned, stat.deaths);
        first = false;
    }
    return trace + "}";
}

std::string energy_trace(const std::vector<int> &energy_at_wave) {
    std::string trace = "[";
    for (std::size_t index = 0; index < energy_at_wave.size(); ++index) {
        trace += (index == 0 ? "" : ",") + std::to_string(energy_at_wave[index]);
    }
    return trace + "]";
}

// One line of the sweep, purpose-built for `analyze_endless.py`: the knobs that were varied, the run length, the
// score and the economy trace. Deliberately not `to_jsonl(report)` -- that line carries a match's detail and none
// of the tuning, and the analysis needs the tuning on every row to group by it.
std::string run_to_jsonl(const std::string &policy, std::uint32_t seed, const KnobSet &knobs, const SimMatchReport &report) {
    return std::format(R"({{"policy":"{}","seed":{},"base_budget":{:.2f},"escalation":{:.4f},"bounty_decay":{:.4f},)"
                       R"("escalation_curve":{:.4f},"hostile_damage_growth":{:.4f},"hostile_damage_cap":{:.2f},"waves_reached":{},"level_score":{},)"
                       R"("clear_time_s":{:.1f},"decided":{},"remaining_integrity":{},"energy_spent":{},"deployments_total":{},"peak_enemies":{},)"
                       R"("energy_at_wave":{},"spawned_deaths":{}}})",
                       policy, seed, knobs.base_budget, knobs.escalation, knobs.bounty_decay, knobs.escalation_curve, knobs.hostile_damage_growth,
                       knobs.hostile_damage_cap, report.waves_reached, report.level_score, report.clear_time_seconds, report.decided ? "true" : "false",
                       report.remaining_integrity, report.energy_spent, report.deployments_total, report.peak_concurrent_enemies,
                       energy_trace(report.energy_at_wave), deaths_trace(report.per_unit));
}

} // namespace

void DefnEndlessRunner::_bind_methods() { ClassDB::bind_static_method(get_class_static(), D_METHOD("run_sweep", "args"), &DefnEndlessRunner::run_sweep); }

Dictionary DefnEndlessRunner::run_sweep(const Dictionary &args) {
    const int seed_count = std::max(static_cast<int>(static_cast<int64_t>(args.get("seeds", 5))), 1);
    const String out_path = args.get("out", String());
    const auto max_seconds = static_cast<double>(args.get("max_seconds", DEFAULT_MAX_SECONDS));

    UnitDataLoader unit_catalog;
    if (!unit_catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS)) {
        return make_failure("DefnEndlessRunner: could not load unit data.");
    }

    UpgradeCatalog upgrade_catalog;
    if (!upgrade_catalog.load(DataPaths::UPGRADES_DATA)) {
        return make_failure("DefnEndlessRunner: could not load upgrades.");
    }

    const auto endless_definition = EndlessScheduleLoader::load(DataPaths::ENDLESS_DATA);
    if (!endless_definition.has_value()) {
        return make_failure("DefnEndlessRunner: could not load the endless definition.");
    }

    const EndlessTuning &shipped = endless_definition->schedule.tuning;
    const std::vector<double> base_budgets = parse_doubles(args.get("base_budget", Array()), shipped.base_budget);
    const std::vector<double> escalations = parse_doubles(args.get("escalation", Array()), shipped.escalation);
    const std::vector<double> decays = parse_doubles(args.get("bounty_decay", Array()), shipped.bounty_decay);
    const std::vector<double> curves = parse_doubles(args.get("escalation_curve", Array()), shipped.escalation_curve);
    const std::vector<double> growths = parse_doubles(args.get("hostile_damage_growth", Array()), shipped.hostile_damage_growth);
    const std::vector<double> caps = parse_doubles(args.get("hostile_damage_cap", Array()), shipped.hostile_damage_cap);
    const std::vector<SimPolicySpec> policies =
        filter_by_label(parse_policies(args.get("policies", Array()), deployable_friendly_ids(unit_catalog)), args.get("policy_labels", Array()));

    const GlobalUnitConfig &globals = unit_catalog.get_globals();
    const std::vector<std::string> base_unit_ids = upgrade_catalog.get_base_unit_ids();
    const std::vector<ProgressionUpgradeCard> upgrade_cards = upgrade_catalog.get_progression_upgrade_cards();
    const std::vector<std::string> owned_upgrades = roster_unlock_upgrades(upgrade_cards);

    String lines;
    int runs = 0;
    int max_wave = 0;
    for (const KnobSet &knobs : expand_knobs(base_budgets, escalations, curves, decays, growths, caps)) {
        EndlessSchedule schedule = endless_definition->schedule;
        schedule.tuning.base_budget = knobs.base_budget;
        schedule.tuning.escalation = knobs.escalation;
        schedule.tuning.escalation_curve = knobs.escalation_curve;
        schedule.tuning.bounty_decay = knobs.bounty_decay;
        schedule.tuning.hostile_damage_growth = knobs.hostile_damage_growth;
        schedule.tuning.hostile_damage_cap = knobs.hostile_damage_cap;

        for (const SimPolicySpec &policy : policies) {
            for (int index = 0; index < seed_count; ++index) {
                SimScenario scenario;
                scenario.level_id = "endless";
                scenario.seed = static_cast<std::uint32_t>(2026 + index);
                scenario.policy = policy;
                scenario.max_seconds = max_seconds;
                scenario.owned_upgrades = owned_upgrades;
                scenario.endless = schedule;

                SimMatch match(unit_catalog, globals, endless_definition->level, scenario, base_unit_ids, upgrade_cards);
                const SimMatchReport report = match.run();
                max_wave = std::max(max_wave, report.waves_reached);
                ++runs;
                lines += to_godot_string(run_to_jsonl(report.policy, scenario.seed, knobs, report)) + "\n";
            }
        }
    }

    if (!out_path.is_empty()) {
        Ref<FileAccess> file = FileAccess::open(out_path, FileAccess::WRITE);
        if (!file.is_valid()) {
            return make_failure(String("DefnEndlessRunner: could not write: ") + out_path);
        }
        file->store_string(lines);
        file->close();
    } else {
        UtilityFunctions::print(lines);
    }

    Dictionary result;
    result["success"] = true;
    result["runs"] = runs;
    result["max_wave"] = max_wave;
    result["out"] = out_path;
    return result;
}

} // namespace defn
