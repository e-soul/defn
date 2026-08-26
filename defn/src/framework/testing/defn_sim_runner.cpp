// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "defn_sim_runner.h"

#include "data_paths.h"
#include "godot_string.h"
#include "json_file_loader.h"
#include "level_loader.h"
#include "sim_match.h"
#include "sim_scenario.h"
#include "unit_data.h"
#include "upgrade_catalog.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <map>
#include <string>
#include <vector>

namespace defn {

namespace {

SimCameraMode parse_camera_mode(const String &value) { return value == String("fixed") ? SimCameraMode::FIXED : SimCameraMode::MODELLED; }

std::vector<std::string> to_string_vector(const Array &values) {
    std::vector<std::string> result;
    result.reserve(values.size());
    for (const Variant &value : values) {
        result.push_back(to_std_string(value));
    }
    return result;
}

std::vector<ScriptedCommand> parse_script(const Array &entries) {
    std::vector<ScriptedCommand> script;
    script.reserve(entries.size());
    for (const Variant &value : entries) {
        const Dictionary entry = value;
        script.push_back({
            .time_seconds = static_cast<double>(entry.get("time", 0.0)),
            .unit_id = to_std_string(entry.get("unit_id", String())),
        });
    }
    return script;
}

std::map<std::string, double> parse_weights(const Dictionary &weights) {
    std::map<std::string, double> parsed;
    const Array unit_ids = weights.keys();
    for (const Variant &unit_id : unit_ids) {
        parsed.emplace(to_std_string(unit_id), static_cast<double>(weights[unit_id]));
    }
    return parsed;
}

SimPolicySpec parse_policy(const Dictionary &policy) {
    SimPolicySpec spec;
    spec.kind = to_std_string(policy.get("kind", String("greedy")));
    spec.energy_reserve = static_cast<int>(static_cast<int64_t>(policy.get("energy_reserve", 15)));
    spec.script = parse_script(policy.get("script", Array()));
    spec.weights = parse_weights(policy.get("weights", Dictionary()));
    return spec;
}

// Expands whatever the file described into a flat list of runs, before seeds multiply it.
std::vector<SimScenario> expand_runs(const Dictionary &data, const SimScenario &base, const std::vector<SimPolicySpec> &policies);

SimScenario parse_scenario(const Dictionary &data) {
    SimScenario scenario;
    scenario.level_id = to_std_string(data.get("level_id", String("level_01")));
    scenario.seed = static_cast<std::uint32_t>(static_cast<int64_t>(data.get("seed", 0)));
    scenario.owned_upgrades = to_string_vector(data.get("owned_upgrades", Array()));
    scenario.camera = parse_camera_mode(data.get("camera", String("modelled")));
    scenario.max_seconds = static_cast<double>(data.get("max_seconds", 300.0));
    scenario.policy = parse_policy(data.get("policy", Dictionary()));
    return scenario;
}

// A scenario may name one level and one policy, lists of each, or an explicit list of runs. Lists are the whole point
// of a sweep: the spread across policies is what stops a single number being mistaken for a verdict, and explicit runs
// let a sweep give each level the upgrades a player would actually have reached it with.
std::vector<std::string> parse_levels(const Dictionary &data, const SimScenario &base) {
    const Array levels = data.get("levels", Array());
    if (levels.is_empty()) {
        return {base.level_id};
    }
    return to_string_vector(levels);
}

// Each entry overrides the defaults from the top of the file, so a run spec only has to say what differs.
std::vector<SimScenario> parse_runs(const Dictionary &data, const SimScenario &base) {
    const Array runs = data.get("runs", Array());
    std::vector<SimScenario> scenarios;
    scenarios.reserve(runs.size());
    for (const Variant &value : runs) {
        const Dictionary entry = value;
        SimScenario scenario = base;
        scenario.level_id = to_std_string(entry.get("level_id", to_godot_string(base.level_id)));
        if (entry.has("owned_upgrades")) {
            scenario.owned_upgrades = to_string_vector(entry.get("owned_upgrades", Array()));
        }
        if (entry.has("policy")) {
            scenario.policy = parse_policy(entry.get("policy", Dictionary()));
        }
        if (entry.has("max_seconds")) {
            scenario.max_seconds = static_cast<double>(entry.get("max_seconds", base.max_seconds));
        }
        scenarios.push_back(scenario);
    }
    return scenarios;
}

std::vector<SimPolicySpec> parse_policies(const Dictionary &data, const SimScenario &base) {
    const Array policies = data.get("policies", Array());
    if (policies.is_empty()) {
        return {base.policy};
    }

    std::vector<SimPolicySpec> specs;
    specs.reserve(policies.size());
    for (const Variant &value : policies) {
        specs.push_back(parse_policy(value));
    }
    return specs;
}

// The shipped world width comes from the background texture, so measure it here rather than guessing in the kernel.
std::optional<float> measure_world_width(const std::string &background_path, const GameplayRules &rules) {
    if (background_path.empty()) {
        return std::nullopt;
    }

    Ref<Texture2D> texture = ResourceLoader::get_singleton()->load(to_godot_string(background_path));
    if (!texture.is_valid()) {
        return std::nullopt;
    }

    const godot::Vector2 size = texture->get_size();
    if (size.y <= 0.0F) {
        return std::nullopt;
    }

    const float display_width = static_cast<float>(size.x) * (rules.viewport_height / static_cast<float>(size.y));
    return display_width * static_cast<float>(rules.world_multiplier);
}

Dictionary make_failure(const String &message) {
    UtilityFunctions::printerr(message);
    Dictionary result;
    result["success"] = false;
    result["message"] = message;
    return result;
}

std::vector<SimScenario> expand_runs(const Dictionary &data, const SimScenario &base, const std::vector<SimPolicySpec> &policies) {
    std::vector<SimScenario> planned = parse_runs(data, base);

    if (planned.empty()) {
        for (const std::string &level_id : parse_levels(data, base)) {
            SimScenario scenario = base;
            scenario.level_id = level_id;
            planned.push_back(scenario);
        }
    } else if (!data.has("policies")) {
        return planned;
    }

    // Every run gets every policy, because the spread is what stops one number being read as a verdict.
    std::vector<SimScenario> expanded;
    expanded.reserve(planned.size() * policies.size());
    for (const SimScenario &scenario : planned) {
        for (const SimPolicySpec &policy : policies) {
            SimScenario copy = scenario;
            copy.policy = policy;
            expanded.push_back(copy);
        }
    }
    return expanded;
}

} // namespace

void DefnSimRunner::_bind_methods() { ClassDB::bind_static_method(get_class_static(), D_METHOD("run_sweep", "args"), &DefnSimRunner::run_sweep); }

Dictionary DefnSimRunner::run_sweep(const Dictionary &args) {
    const String scenario_path = args.get("scenario", String());
    const int seed_count = std::max(static_cast<int>(static_cast<int64_t>(args.get("seeds", 1))), 1);
    const String out_path = args.get("out", String());

    const auto scenario_data = JsonFileLoader::load_dictionary(scenario_path, "DefnSimRunner");
    if (!scenario_data) {
        return make_failure(String("DefnSimRunner: could not read scenario: ") + scenario_path);
    }

    UnitDataLoader unit_catalog;
    if (!unit_catalog.load(DataPaths::UNIT_DATA, DataPaths::UNIT_GLOBALS)) {
        return make_failure("DefnSimRunner: could not load unit data.");
    }

    UpgradeCatalog upgrade_catalog;
    if (!upgrade_catalog.load(DataPaths::UPGRADES_DATA)) {
        return make_failure("DefnSimRunner: could not load upgrades.");
    }

    const SimScenario base_scenario = parse_scenario(*scenario_data);
    const std::vector<SimPolicySpec> policies = parse_policies(*scenario_data, base_scenario);

    const std::vector<SimScenario> planned = expand_runs(*scenario_data, base_scenario, policies);

    const GlobalUnitConfig &globals = unit_catalog.get_globals();
    const std::vector<std::string> base_unit_ids = upgrade_catalog.get_base_unit_ids();
    const std::vector<ProgressionUpgradeCard> upgrade_cards = upgrade_catalog.get_progression_upgrade_cards();

    String lines;
    int victories = 0;
    int runs = 0;
    for (const SimScenario &planned_scenario : planned) {
        const auto level = LevelLoader::load(DataPaths::level_definition(to_godot_string(planned_scenario.level_id)));
        if (!level) {
            return make_failure(String("DefnSimRunner: could not load level: ") + to_godot_string(planned_scenario.level_id));
        }

        const std::optional<float> world_width = measure_world_width(level->background_path, globals.gameplay_rules);
        for (int index = 0; index < seed_count; ++index) {
            SimScenario scenario = planned_scenario;
            scenario.seed = planned_scenario.seed + static_cast<std::uint32_t>(index);
            scenario.world_width = world_width;

            SimMatch match(unit_catalog, globals, *level, scenario, base_unit_ids, upgrade_cards);
            const SimMatchReport report = match.run();
            victories += report.victory ? 1 : 0;
            ++runs;
            lines += to_godot_string(to_jsonl(report)) + "\n";
        }
    }

    if (!out_path.is_empty()) {
        Ref<FileAccess> file = FileAccess::open(out_path, FileAccess::WRITE);
        if (!file.is_valid()) {
            return make_failure(String("DefnSimRunner: could not write: ") + out_path);
        }
        file->store_string(lines);
        file->close();
    } else {
        UtilityFunctions::print(lines);
    }

    Dictionary result;
    result["success"] = true;
    result["runs"] = runs;
    result["victories"] = victories;
    result["out"] = out_path;
    return result;
}

} // namespace defn
