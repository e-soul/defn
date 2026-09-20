"""Declarative runner options and the shared, once-per-invocation Godot preparation."""

from dataclasses import dataclass, field
from pathlib import Path
import math
import os
import shutil
import subprocess


def boolean(value):
    value = str(value).lower()
    if value not in {"0", "1", "false", "true", "no", "yes", "off", "on"}:
        raise ValueError(f"Expected a boolean, got {value!r}")
    return value in {"1", "true", "yes", "on"}


@dataclass(frozen=True)
class Option:
    flag: str
    kind: str = "text"

    def validate(self, value):
        if not str(value).strip():
            raise ValueError(f"{self.flag} requires a value")
        if self.kind == "bool":
            boolean(value)
        elif self.kind == "positive_int":
            if int(value) <= 0:
                raise ValueError(f"{self.flag} must be a positive integer")
        elif self.kind in {"number", "numbers", "positive"}:
            values = str(value).split(",") if self.kind == "numbers" else [value]
            for part in values:
                number = float(part)
                if not math.isfinite(number) or number < 0 or (self.kind == "positive" and number == 0):
                    raise ValueError(f"{self.flag} requires finite nonnegative values")


@dataclass(frozen=True)
class Runner:
    script: str
    isolation: str
    options: dict = field(default_factory=dict)
    engine_args: tuple = ()


COMMON = {"seeds": Option("--seeds", "positive_int"), "out": Option("--out")}
ENDLESS = {
    name: Option("--" + flag, "numbers")
    for name, flag in {
        "base_budget": "base-budget", "escalation": "escalation",
        "decay": "bounty-decay", "decay_curve": "bounty-decay-curve",
        "bounty_floor": "bounty-floor", "curve": "escalation-curve",
        "dmg": "hostile-damage-growth", "dmg_cap": "hostile-damage-cap",
        "elite": "elite-fraction-cap", "elite_hp": "elite-hp-growth",
        "elite_start": "elite-first-wave", "elite_hp_base": "elite-hp",
        "interval": "wave-interval", "interval_growth": "interval-growth",
        "supply_start": "supply-start", "supply_growth": "supply-growth",
        "supply": "supply-cap", "energy_cap": "energy-cap",
    }.items()
}
RUNNERS = {
    "hosted_test": Runner("godot_hosted_runner.gd", "tests"),
    "conformance": Runner("godot_conformance_runner.gd", "conformance", engine_args=("--fixed-fps", "60")),
    "sim": Runner("godot_sim_runner.gd", "sim", {
        **COMMON, "scenario": Option("--scenario"), "bisect": Option("--bisect", "bool"),
    }),
    "balance": Runner("godot_balance_runner.gd", "balance", COMMON),
    "matrix": Runner("godot_matrix_runner.gd", "matrix", {
        **COMMON, "spec": Option("--spec"), "separation": Option("--separation", "number"),
        "spacing": Option("--friendly-spacing", "number"),
    }),
    "endless": Runner("godot_endless_runner.gd", "endless", {
        **COMMON, **ENDLESS, "max_seconds": Option("--max-seconds", "positive"),
        "policies": Option("--policies"),
    }),
}
RUNNER_OPTIONS = set().union(*(runner.options for runner in RUNNERS.values()))


def validate_options(arguments, selected):
    supported = set().union(*(RUNNERS[name].options for name in selected))
    supplied = RUNNER_OPTIONS.intersection(arguments)
    unsupported = supplied - supported
    if unsupported:
        raise ValueError("Options not supported by the selected runners: " + ", ".join(sorted(unsupported)))
    # A single explicit output would let parallel runners overwrite one another.
    if "out" in supplied and sum("out" in RUNNERS[name].options for name in selected) > 1:
        raise ValueError("out= requires selecting only one measurement runner")
    for name in selected:
        for key, option in RUNNERS[name].options.items():
            if key in arguments:
                try:
                    option.validate(arguments[key])
                except ValueError as error:
                    raise ValueError(f"Invalid {key}={arguments[key]}: {error}") from error


def runner_arguments(name, arguments):
    result = []
    for key, option in RUNNERS[name].options.items():
        if key in arguments:
            result.extend([option.flag, str(arguments[key])])
    return result


def resolve_godot(configured=""):
    for candidate in [configured, os.environ.get("GODOT_BIN", ""), "godot4", "godot"]:
        if candidate:
            found = shutil.which(candidate)
            if found:
                return found
            if Path(candidate).is_file():
                return str(Path(candidate).resolve())
    raise ValueError("Unable to locate Godot. Set godot_bin=<path> or GODOT_BIN.")


def isolated_environment(name):
    root = Path("build") / "hosted_test_user" / name
    if root.exists():
        shutil.rmtree(root)
    result = os.environ.copy()
    for variable, directory in {
        "APPDATA": "Roaming", "LOCALAPPDATA": "Local", "XDG_CONFIG_HOME": "xdg_config",
        "XDG_DATA_HOME": "xdg_data", "XDG_CACHE_HOME": "xdg_cache",
    }.items():
        path = root / directory
        path.mkdir(parents=True, exist_ok=True)
        result[variable] = str(path.resolve())
    return result


def write_success(target):
    path = Path(str(target[0]))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("success\n", encoding="ascii")


def prepare_project(target, source, env):
    project = Path.cwd()
    extension_list = project / ".godot" / "extension_list.cfg"
    extension_list.parent.mkdir(parents=True, exist_ok=True)
    extension_list.write_text("res://defn_core.gdextension\n", encoding="ascii")
    run_env = isolated_environment("prepare")
    # An instrumented DLL also runs during import; do not pollute coverage input.
    run_env["LLVM_PROFILE_FILE"] = str((project / "build" / "hosted_test_user" / "prepare" / "import-%p.profraw"))
    result = subprocess.run(
        [env["DEFN_GODOT_BIN"], "--headless", "--path", str(project), "--import"],
        check=False, cwd=project, env=run_env,
    )
    if result.returncode == 0:
        write_success(target)
    return result.returncode


def execute_runner(name, env, run_env=None):
    runner = RUNNERS[name]
    project = Path.cwd()
    command = [
        env["DEFN_GODOT_BIN"], "--headless", *runner.engine_args,
        "--path", str(project), "--script", str(project / "tests" / runner.script),
    ]
    arguments = runner_arguments(name, env.get("DEFN_RUNNER_OPTIONS", {}))
    if arguments:
        command += ["--", *arguments]
    if run_env is None:
        run_env = isolated_environment(runner.isolation)
        run_env["LLVM_PROFILE_FILE"] = str(
            (project / "build" / "hosted_test_user" / runner.isolation / "run-%p.profraw"))
    return subprocess.run(
        command, check=False, cwd=project,
        env=run_env,
    ).returncode


def run_runner(target, source, env):
    result = execute_runner(env["DEFN_RUNNER"], env)
    if result == 0:
        write_success(target)
    return result


def add_preparation(env, staged):
    from SCons.Action import Action
    env["DEFN_GODOT_BIN"] = resolve_godot(env.get("DEFN_GODOT_BIN", ""))
    node = env.Command("build/godot_prepare.stamp", staged,
                       Action(prepare_project, "Preparing Godot project (one import)"))
    env.AlwaysBuild(node)
    env.NoCache(node)
    return node


def add_runners(env, names, preparation):
    from SCons.Action import Action
    result = {}
    for name in sorted(names):
        runner_env = env.Clone(DEFN_RUNNER=name)
        node = runner_env.Command(
            f"build/runners/{name}.stamp",
            [preparation, "tests/" + RUNNERS[name].script],
            Action(run_runner, f"Running {name}"),
        )
        runner_env.AlwaysBuild(node)
        runner_env.NoCache(node)
        runner_env.Alias(name, node)
        result[name] = node
    return result
