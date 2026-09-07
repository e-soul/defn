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
#include <array>
#include <cstddef>
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

// The composition a transitioning player is expected to pass through, and the waves they pass through it at.
//
// Authored rather than derived, and it is the drift schedule's own question read back: the light half of the roster
// answers an opening of evasive swarm, and the heavy half is what the armoured tail needs. The waves are where
// `data/endless.json` moves its keyframes, so this policy changes its mind roughly when the hostiles do.
std::vector<MixKeyframe> transition_keyframes(const std::vector<std::string> &friendly_unit_ids) {
    const auto has = [&friendly_unit_ids](const char *unit_id) { return std::ranges::find(friendly_unit_ids, unit_id) != friendly_unit_ids.end(); };
    if (!has("operator") || !has("breacher") || !has("marksman") || !has("impact")) {
        return {};
    }

    return {
        {.wave = 1, .weights = {{"operator", 2.0}, {"breacher", 1.0}}},
        {.wave = 12, .weights = {{"operator", 1.0}, {"breacher", 1.0}, {"impact", 1.0}}},
        {.wave = 22, .weights = {{"breacher", 1.0}, {"impact", 1.0}, {"marksman", 1.0}}},
        {.wave = 32, .weights = {{"breacher", 1.0}, {"marksman", 2.0}}},
    };
}

// The default slate. The three standard policies say how long a run lasts; the mono mixes and pairs are the
// not-solved gate, and there has to be one per friendly unit or the gate cannot see the mix that solves the mode.
//
// `transition` is the row the gate is actually about. Every other entry fixes its composition for the whole run,
// and a fixed composition is what a drifting schedule is built to punish -- so without this the sweep measures how
// long each wrong answer survives and never measures the right one. `ENDLESS_MODE.md` recorded the mode's ceiling
// as unmeasured for exactly this reason.
std::vector<SimPolicySpec> default_policies(const std::vector<std::string> &friendly_unit_ids) {
    std::vector<SimPolicySpec> policies = {
        {.kind = "greedy", .label = "greedy"},
        {.kind = "defensive", .label = "defensive"},
        {.kind = "patience", .label = "patience"},
    };

    if (std::vector<MixKeyframe> keyframes = transition_keyframes(friendly_unit_ids); !keyframes.empty()) {
        policies.push_back({.kind = "transition", .transition = std::move(keyframes), .label = "transition"});
    }

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
    double elite_fraction_cap = 0.0;
    double elite_hp_growth = 0.0;
    double elite_first_wave = 0.0;
    double wave_interval = 0.0;
    double interval_growth = 0.0;
    double supply_start = 0.0;
    double supply_growth = 0.0;
    double elite_hp = 0.0;
    // Level rules rather than schedule ones, and swept here anyway: they are what the schedule is tuned *against*,
    // so a cell that does not carry them is measuring a different game.
    double supply_cap = 0.0;
    double energy_cap = 0.0;
};

// The knobs a cell varies, each as the list of values to try. Bundled because the expansion below is a product over
// all of them and a positional parameter list of ten `std::vector<double>` is a bug waiting to be written.
constexpr std::size_t KNOB_AXIS_COUNT = 16;

struct KnobAxes {
    std::vector<double> base_budgets;
    std::vector<double> escalations;
    std::vector<double> curves;
    std::vector<double> decays;
    std::vector<double> growths;
    std::vector<double> damage_caps;
    std::vector<double> elite_fractions;
    std::vector<double> elite_hp_growths;
    std::vector<double> elite_hps;
    std::vector<double> elite_first_waves;
    std::vector<double> wave_intervals;
    std::vector<double> interval_growths;
    std::vector<double> supply_starts;
    std::vector<double> supply_growths;
    std::vector<double> supply_caps;
    std::vector<double> energy_caps;
};

std::vector<KnobSet> expand_knobs(const KnobAxes &axes) {
    // An odometer over the axes rather than one loop per knob. The nested form was already the deepest function in
    // this file at six knobs, and every knob added made it worse; this way a new axis is one entry in each of the
    // two tables below and the control flow never changes.
    const std::array<const std::vector<double> *, KNOB_AXIS_COUNT> values = {
        &axes.base_budgets,  &axes.escalations,       &axes.curves,          &axes.decays,
        &axes.growths,       &axes.damage_caps,       &axes.elite_fractions, &axes.elite_hp_growths,
        &axes.elite_hps,     &axes.elite_first_waves, &axes.wave_intervals,  &axes.interval_growths,
        &axes.supply_starts, &axes.supply_growths,    &axes.supply_caps,     &axes.energy_caps,
    };

    // `parse_doubles` substitutes the shipped value for an omitted flag, so every axis holds at least one entry.
    // An empty one would mean a caller built `KnobAxes` by hand and left an axis out, and the product of zero cells
    // is no sweep rather than a sweep of the defaults -- say so by returning nothing instead of indexing past it.
    std::size_t total = 1;
    for (const std::vector<double> *axis : values) {
        if (axis->empty()) {
            return {};
        }
        total *= axis->size();
    }

    std::vector<KnobSet> cells;
    cells.reserve(total);
    std::array<std::size_t, KNOB_AXIS_COUNT> cursor{};
    for (std::size_t cell = 0; cell < total; ++cell) {
        KnobSet knobs;
        const std::array<double *, KNOB_AXIS_COUNT> fields = {
            &knobs.base_budget,        &knobs.escalation,         &knobs.escalation_curve, &knobs.bounty_decay,  &knobs.hostile_damage_growth,
            &knobs.hostile_damage_cap, &knobs.elite_fraction_cap, &knobs.elite_hp_growth,  &knobs.elite_hp,      &knobs.elite_first_wave,
            &knobs.wave_interval,      &knobs.interval_growth,    &knobs.supply_start,     &knobs.supply_growth, &knobs.supply_cap,
            &knobs.energy_cap,
        };
        for (std::size_t axis = 0; axis < KNOB_AXIS_COUNT; ++axis) {
            *fields.at(axis) = values.at(axis)->at(cursor.at(axis));
        }
        cells.push_back(knobs);

        // Carry, least significant axis first, so the last axis varies fastest and a sweep reads in the order it
        // was written on the command line.
        for (std::size_t axis = KNOB_AXIS_COUNT; axis-- > 0;) {
            if (++cursor.at(axis) < values.at(axis)->size()) {
                break;
            }
            cursor.at(axis) = 0;
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

std::string int_trace(const std::vector<int> &values) {
    std::string trace = "[";
    for (std::size_t index = 0; index < values.size(); ++index) {
        trace += (index == 0 ? "" : ",") + std::to_string(values[index]);
    }
    return trace + "]";
}

// One line of the sweep, purpose-built for `analyze_endless.py`: the knobs that were varied, the run length, the
// score and the economy trace. Deliberately not `to_jsonl(report)` -- that line carries a match's detail and none
// of the tuning, and the analysis needs the tuning on every row to group by it.
std::string run_to_jsonl(const std::string &policy, std::uint32_t seed, const KnobSet &knobs, const SimMatchReport &report) {
    return std::format(R"({{"policy":"{}","seed":{},"base_budget":{:.2f},"escalation":{:.4f},"bounty_decay":{:.4f},)"
                       R"("escalation_curve":{:.4f},"hostile_damage_growth":{:.4f},"hostile_damage_cap":{:.2f},)"
                       R"("elite_fraction_cap":{:.4f},"elite_hp_growth":{:.4f},"elite_hp":{:.2f},"elite_first_wave":{},)"
                       R"("wave_interval":{:.2f},"interval_growth":{:.4f},"supply_start":{:.1f},"supply_growth":{:.3f},)"
                       R"("supply_cap":{},"energy_cap":{},)"
                       R"("waves_reached":{},"level_score":{},)"
                       R"("clear_time_s":{:.1f},"decided":{},"remaining_integrity":{},"energy_spent":{},"deployments_total":{},)"
                       R"("deployments_blocked":{},"peak_friendlies":{},"peak_enemies":{},"first_capped_wave":{},)"
                       R"("energy_at_wave":{},"friendly_deaths_at_wave":{},"spawned_deaths":{}}})",
                       policy, seed, knobs.base_budget, knobs.escalation, knobs.bounty_decay, knobs.escalation_curve, knobs.hostile_damage_growth,
                       knobs.hostile_damage_cap, knobs.elite_fraction_cap, knobs.elite_hp_growth, knobs.elite_hp, static_cast<int>(knobs.elite_first_wave),
                       knobs.wave_interval, knobs.interval_growth, knobs.supply_start, knobs.supply_growth, static_cast<int>(knobs.supply_cap),
                       static_cast<int>(knobs.energy_cap), report.waves_reached, report.level_score, report.clear_time_seconds,
                       report.decided ? "true" : "false", report.remaining_integrity, report.energy_spent, report.deployments_total, report.deployments_blocked,
                       report.peak_friendlies, report.peak_concurrent_enemies, report.first_capped_wave, int_trace(report.energy_at_wave),
                       int_trace(report.friendly_deaths_at_wave), deaths_trace(report.per_unit));
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
    const KnobAxes axes{
        .base_budgets = parse_doubles(args.get("base_budget", Array()), shipped.base_budget),
        .escalations = parse_doubles(args.get("escalation", Array()), shipped.escalation),
        .curves = parse_doubles(args.get("escalation_curve", Array()), shipped.escalation_curve),
        .decays = parse_doubles(args.get("bounty_decay", Array()), shipped.bounty_decay),
        .growths = parse_doubles(args.get("hostile_damage_growth", Array()), shipped.hostile_damage_growth),
        .damage_caps = parse_doubles(args.get("hostile_damage_cap", Array()), shipped.hostile_damage_cap),
        .elite_fractions = parse_doubles(args.get("elite_fraction_cap", Array()), shipped.elite_fraction_cap),
        .elite_hp_growths = parse_doubles(args.get("elite_hp_growth", Array()), shipped.elite_hp_growth),
        .elite_hps = parse_doubles(args.get("elite_hp", Array()), shipped.elite_hp),
        .elite_first_waves = parse_doubles(args.get("elite_first_wave", Array()), shipped.elite_first_wave),
        .wave_intervals = parse_doubles(args.get("wave_interval", Array()), shipped.wave_interval),
        .interval_growths = parse_doubles(args.get("interval_growth", Array()), shipped.interval_growth),
        .supply_starts = parse_doubles(args.get("supply_start", Array()), shipped.supply_start),
        .supply_growths = parse_doubles(args.get("supply_growth", Array()), shipped.supply_growth),
        .supply_caps = parse_doubles(args.get("supply_cap", Array()), endless_definition->level.supply_cap),
        .energy_caps = parse_doubles(args.get("energy_cap", Array()), endless_definition->level.energy_cap),
    };
    const std::vector<SimPolicySpec> policies =
        filter_by_label(parse_policies(args.get("policies", Array()), deployable_friendly_ids(unit_catalog)), args.get("policy_labels", Array()));

    const GlobalUnitConfig &globals = unit_catalog.get_globals();
    const std::vector<std::string> base_unit_ids = upgrade_catalog.get_base_unit_ids();
    const std::vector<ProgressionUpgradeCard> upgrade_cards = upgrade_catalog.get_progression_upgrade_cards();
    const std::vector<std::string> owned_upgrades = roster_unlock_upgrades(upgrade_cards);

    String lines;
    int runs = 0;
    int max_wave = 0;
    for (const KnobSet &knobs : expand_knobs(axes)) {
        EndlessSchedule schedule = endless_definition->schedule;
        schedule.tuning.base_budget = knobs.base_budget;
        schedule.tuning.escalation = knobs.escalation;
        schedule.tuning.escalation_curve = knobs.escalation_curve;
        schedule.tuning.bounty_decay = knobs.bounty_decay;
        schedule.tuning.hostile_damage_growth = knobs.hostile_damage_growth;
        schedule.tuning.hostile_damage_cap = knobs.hostile_damage_cap;
        schedule.tuning.elite_fraction_cap = knobs.elite_fraction_cap;
        schedule.tuning.elite_hp_growth = knobs.elite_hp_growth;
        schedule.tuning.elite_hp = knobs.elite_hp;
        schedule.tuning.elite_first_wave = static_cast<int>(knobs.elite_first_wave);
        schedule.tuning.wave_interval = knobs.wave_interval;
        schedule.tuning.interval_growth = knobs.interval_growth;
        schedule.tuning.supply_start = knobs.supply_start;
        schedule.tuning.supply_growth = knobs.supply_growth;

        // The caps are level rules, so a cell varies the level rather than the schedule. Both are part of the same
        // measurement: what the mode asks and what the player is allowed to answer with.
        LevelDefinition level = endless_definition->level;
        level.supply_cap = static_cast<int>(knobs.supply_cap);
        level.energy_cap = static_cast<int>(knobs.energy_cap);

        for (const SimPolicySpec &policy : policies) {
            for (int index = 0; index < seed_count; ++index) {
                SimScenario scenario;
                scenario.level_id = "endless";
                scenario.seed = static_cast<std::uint32_t>(2026 + index);
                scenario.policy = policy;
                scenario.max_seconds = max_seconds;
                scenario.owned_upgrades = owned_upgrades;
                scenario.endless = schedule;

                SimMatch match(unit_catalog, globals, level, scenario, base_unit_ids, upgrade_cards);
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
