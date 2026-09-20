"""Native checks, packaging and persistent per-translation-unit clang-tidy."""

from pathlib import Path
import json
import os
import shutil
import subprocess
import sys

from .runners import write_success
from .configuration import tool_identity


def run_native(target, source, env):
    result = subprocess.run([source[0].abspath], check=False)
    if result.returncode == 0:
        write_success(target)
    return result.returncode


def run_packaging(target, source, env):
    result = subprocess.run([sys.executable, source[0].abspath], check=False)
    if result.returncode == 0:
        write_success(target)
    return result.returncode


def add_check(env, name, inputs, function, path=None):
    from SCons.Action import Action
    node = env.Command(path or f"build/{name}.stamp", inputs, Action(function, f"Running {name}"))
    env.AlwaysBuild(node)
    env.NoCache(node)
    env.Alias(name, node)
    return node


def normalize_command(command):
    return command.replace(" -fno-gnu-unique", "")


def compilation_entry(env, source, obj):
    from SCons.Action import Action
    from SCons.Tool.compilation_db import CompDBTEMPFILE
    command = Action("$SHCXXCOM").strfunction(
        target=[obj], source=[env.File(source)], env=env,
        overrides={"TEMPFILE": CompDBTEMPFILE},
    )
    return {"directory": str(Path.cwd()), "file": str(Path(source).resolve()),
            "command": normalize_command(command), "output": obj.abspath}


def write_tidy_database(target, source, env):
    path = Path(str(target[0]))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(env["DEFN_TIDY_ENTRY"], encoding="utf-8")
    return 0


def run_tidy(target, source, env):
    stamp = Path(str(target[0]))
    stamp.unlink(missing_ok=True)
    database = Path(env["DEFN_TIDY_DATABASE"]) / "compile_commands.json"
    if not database.is_file():
        print(f"Missing clang-tidy compilation database: {database}")
        return 1
    result = subprocess.run([
        env["DEFN_TIDY_TOOL"], "--quiet", "--warnings-as-errors=*",
        "--extra-arg=-Qunused-arguments", "-p", env["DEFN_TIDY_DATABASE"],
        source[0].abspath,
    ], check=False, env=env["ENV"])
    if result.returncode == 0:
        write_success(target)
    return result.returncode


def add_tidy(env, root, inventory, objects):
    from SCons.Action import Action
    from SCons.Scanner.C import CScanner
    tool = shutil.which("clang-tidy", path=env["ENV"].get("PATH", os.environ.get("PATH", "")))
    if not tool:
        raise ValueError("Unable to locate clang-tidy; add LLVM to PATH")
    version = subprocess.run([tool, "--version"], capture_output=True, text=True, check=True).stdout
    nodes = []
    for source, obj in zip(inventory, objects):
        directory = root / "tidy" / Path(source).with_suffix("")
        entry = json.dumps([compilation_entry(env, source, obj)], sort_keys=True)
        check_env = env.Clone(DEFN_TIDY_ENTRY=entry, DEFN_TIDY_TOOL=tool,
                              DEFN_TIDY_DATABASE=str(directory.resolve()))
        database = check_env.Command(
            str(directory / "compile_commands.json"), check_env.Value(entry),
            Action(write_tidy_database, "Preparing tidy command for " + source),
        )
        node = check_env.Command(
            str(directory / "success.stamp"), source,
            Action(run_tidy, "Running clang-tidy on $SOURCE"),
            source_scanner=CScanner(),
        )
        # Recursive C scanning gives the same transitive include graph as objects,
        # without requiring compilation. The command dependency is per TU, not DB-wide.
        identity = tool_identity(tool, env["ENV"].get("PATH", ""))
        check_env.Depends(node, [database, ".clang-tidy", check_env.Value((identity, version, entry))])
        check_env.NoCache(node)
        nodes.extend(node)
    env.Alias("tidy", nodes)
    return nodes
