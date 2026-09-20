"""Artifact construction and content-aware staging to the public bin paths."""

from pathlib import Path
import shutil

from .configuration import variant_identity
from . import sources


def stage_file(destination, source):
    destination, source = Path(destination), Path(source)
    if destination.exists() and destination.stat().st_size == source.stat().st_size:
        # Do not use filecmp's stat-keyed cache: copy2 preserves timestamps, and
        # switching equal-sized variants can otherwise reuse a stale comparison.
        with destination.open("rb") as current, source.open("rb") as selected:
            while True:
                block = current.read(1024 * 1024)
                if block != selected.read(1024 * 1024):
                    break
                if not block:
                    return False
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    return True


def stage_action(target, source, env):
    for destination, artifact in zip(target, source):
        stage_file(str(destination), str(artifact))
    if env.get("DEFN_REMOVE_STALE_PDB", False):
        Path(str(target[0])).with_suffix(".pdb").unlink(missing_ok=True)
    return 0


def stage(env, artifact, destination):
    from SCons.Action import Action
    destinations = [str(path) for path in destination] if isinstance(destination, list) else str(destination)
    node = env.Command(destinations, artifact, Action(stage_action, "Checking staged $TARGET"))
    # The public path is shared by variants. SCons' previous signature alone
    # cannot tell which variant another invocation left there.
    env.AlwaysBuild(node)
    env.NoCache(node)
    return node


def link_options(env, binary):
    if env.get("is_msvc", False) and env.get("debug_symbols", False):
        return {"PDB": str(binary.with_suffix(".pdb"))}
    return {}


def stage_binary(env, artifacts):
    binary = artifacts[0]
    symbols = [node for node in artifacts[1:] if str(node).endswith(".pdb")]
    selected = [binary, *symbols]
    stage_env = env.Clone(DEFN_REMOVE_STALE_PDB=env["platform"] == "windows" and not symbols)
    return stage(stage_env, selected, [Path("bin") / Path(str(node)).name for node in selected])


def objects(env, root, inventory, shared=False):
    builder = env.SharedObject if shared else env.Object
    return [builder(target=str(root / "obj" / Path(src).with_suffix("")), source=src)[0]
            for src in inventory]


def extension(env, hosted=False, coverage=False):
    if hosted:
        env.AppendUnique(CPPPATH=["tests"], CPPDEFINES=["DEFN_HOSTED_TESTS_ENABLED"])
    root = variant_identity(env, "hosted-coverage" if coverage else "hosted" if hosted else "extension")
    inventory = sources.extension_sources(env["target"] == "template_debug", hosted)
    built_objects = objects(env, root, inventory, shared=True)
    name = "defn_core" + env["suffix"] + env["SHLIBSUFFIX"]
    binary = root / "lib" / name
    library = env.SharedLibrary(target=str(binary), source=built_objects, **link_options(env, binary))
    # SharedLibrary applies the platform prefix (lib on Linux/Web).
    staged = stage_binary(env, library)
    return library, staged, root, inventory, built_objects


def native(env, coverage=False):
    root = variant_identity(env, "native-coverage" if coverage else "native-tests")
    built_objects = objects(env, root, sources.NATIVE)
    name = "defn_unit_tests_coverage" if coverage else "defn_unit_tests"
    binary = root / "bin" / name
    program = env.Program(target=str(binary), source=built_objects, **link_options(env, binary))
    staged = stage_binary(env, program)
    return program, staged, root
