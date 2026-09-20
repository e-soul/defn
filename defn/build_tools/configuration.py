"""Toolchain setup without coupling the native environment to godot-cpp."""

from functools import lru_cache
from pathlib import Path
import hashlib
import json
import os
import platform
import shlex
import shutil
import subprocess
import sys

from .runners import boolean
from .sources import include_paths


def stable_hash(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, default=str).encode()).hexdigest()[:16]


@lru_cache(maxsize=None)
def tool_identity(command, search_path):
    executable = shutil.which(command, path=search_path)
    if not executable:
        return {"command": command, "missing": True}
    path = Path(executable).resolve()
    stat = path.stat()
    return {"command": command, "path": str(path), "size": stat.st_size, "mtime_ns": stat.st_mtime_ns}


def variant_identity(env, kind):
    keys = (
        "platform", "arch", "target", "precision", "threads", "use_static_cpp",
        "debug_crt", "use_llvm", "use_mingw", "lto", "optimize", "debug_symbols",
        "dev_build", "use_hot_reload", "CCFLAGS", "CXXFLAGS", "CPPDEFINES",
        "LINKFLAGS", "SHLINKFLAGS", "ARFLAGS", "LIBS", "ENV",
    )
    configuration = {key: env.get(key) for key in keys if key != "ENV"}
    search_path = env.get("ENV", {}).get("PATH", os.environ.get("PATH", ""))
    tools = {key: tool_identity(env.subst("$" + key), search_path)
             for key in ("CC", "CXX", "LINK", "SHLINK", "AR") if env.get(key)}
    # SDK selection can change ABI without changing the compiler executable.
    configuration["sdk"] = {key: env.get("ENV", {}).get(key, os.environ.get(key, ""))
                            for key in ("INCLUDE", "LIB", "WindowsSDKVersion", "VCToolsVersion", "SDKROOT")}
    compiler = Path(env.subst("$CXX")).stem
    return (Path("build") / f"{env['platform']}-{env['arch']}"
            / env["target"] / f"{compiler}-{stable_hash((configuration, tools))}" / kind)


def remove_flags(flags, excluded):
    return [flag for flag in flags if str(flag) not in excluded]


def prepare_cache_parent():
    cache = os.environ.get("SCONS_CACHE")
    if cache:
        # SCons initializes the cache atomically, but requires its parent to exist.
        Path(cache).parent.mkdir(parents=True, exist_ok=True)
    return cache


def configure_project(env):
    env.AppendUnique(CPPPATH=include_paths())
    env["CXXFLAGS"] = remove_flags(env.get("CXXFLAGS", []), {"/std:c++17", "-std=c++17", "-fno-exceptions"})
    env["CPPDEFINES"] = [
        define for define in env.get("CPPDEFINES", [])
        if (str(define[0]) if isinstance(define, (tuple, list)) else str(define).split("=")[0]) != "_HAS_EXCEPTIONS"
    ]
    if env.get("is_msvc", False):
        env.Append(CXXFLAGS=["/std:c++latest", "/EHsc"])
    elif env["platform"] == "web":
        env.Append(CXXFLAGS=["-std=c++23", "-fno-exceptions"])
        env["SHLIBPREFIX"] = "lib"
        env["TEMPFILEARGESCFUNC"] = lambda arg: shlex.quote(str(arg))
        env["SHLINKCOM_POSIX"] = env["SHLINKCOM"].replace("$TARGET", "$TARGET.posix").replace("$SOURCES", "$SOURCES.posix")
        env["SHLINKCOM"] = "${TEMPFILE(SHLINKCOM_POSIX)}"
    else:
        env.Append(CXXFLAGS=["-std=c++23", "-fexceptions"])
    if env["target"] == "template_debug":
        env.Append(CPPDEFINES=["DEFN_DEBUG_RENDERING_ENABLED"])
    return env


NATIVE_OPTIONS = {
    "platform", "arch", "target", "use_llvm", "use_static_cpp", "debug_crt",
    "debug_symbols", "dev_build", "optimize", "lto", "verbose", "compiledb",
    "compiledb_file", "precision", "threads", "silence_msvc",
}

EXTENSION_OPTIONS = {
    "profile", "custom_tools", "platform", "target", "gdextension_dir", "custom_api_file",
    "generate_bindings", "generate_template_get_node", "build_library", "precision", "arch",
    "threads", "compiledb", "compiledb_file", "build_profile", "use_hot_reload",
    "disable_exceptions", "symbols_visibility", "optimize", "lto", "debug_symbols",
    "dev_build", "verbose", "mingw_prefix", "use_mingw", "use_static_cpp", "silence_msvc",
    "debug_crt", "use_llvm", "macos_deployment_target", "macos_sdk_path", "osxcross_sdk",
    "android_api_level", "ndk_version", "ANDROID_HOME", "ios_min_version",
    "IOS_TOOLCHAIN_PATH", "IOS_SDK_PATH", "ios_simulator", "ios_triple",
}


def validate_extension_options(arguments):
    # Construction variables such as CCFLAGS happen to exist in the environment,
    # but are not upstream command-line options and would otherwise be ignored.
    unknown = set(arguments) - EXTENSION_OPTIONS
    if unknown:
        raise ValueError("Unsupported build options: " + ", ".join(sorted(unknown)))


def native_environment(arguments):
    from SCons.Script import Environment
    unknown = set(arguments) - NATIVE_OPTIONS
    if unknown:
        raise ValueError("Unsupported native build options: " + ", ".join(sorted(unknown)))
    host = "windows" if os.name == "nt" else "macos" if sys.platform == "darwin" else "linux"
    requested_platform = arguments.get("platform", host)
    if requested_platform != host:
        raise ValueError(f"Native tests run on the host ({host}); platform={requested_platform} is not supported")
    machine = platform.machine().lower()
    native_arch = {
        "amd64": "x86_64", "aarch64": "arm64",
        "i386": "x86_32", "i686": "x86_32", "x86": "x86_32",
    }.get(machine, machine)
    arch = arguments.get("arch", native_arch)
    arch = {"x64": "x86_64", "amd64": "x86_64", "aarch64": "arm64"}.get(arch, arch)
    if arch not in {"x86_64", "x86_32", "arm64"}:
        raise ValueError(f"Unsupported native architecture: {arch}")
    if arch != native_arch and not (native_arch == "x86_64" and arch == "x86_32"):
        raise ValueError(f"Native tests require a host-compatible architecture ({native_arch}); arch={arch} is not supported")
    target = arguments.get("target", "template_debug")
    if target not in {"template_debug", "template_release", "editor"}:
        raise ValueError(f"Unsupported target={target}")
    if arguments.get("precision", "single") not in {"single", "double"}:
        raise ValueError("precision must be single or double")
    boolean(arguments.get("threads", "yes"))
    boolean(arguments.get("silence_msvc", "yes"))
    dev = boolean(arguments.get("dev_build", "no"))
    debug = boolean(arguments.get("debug_symbols", str(dev)))
    llvm = boolean(arguments.get("use_llvm", "no"))
    static = boolean(arguments.get("use_static_cpp", "yes"))
    debug_crt = boolean(arguments.get("debug_crt", "no"))
    optimize = arguments.get("optimize", "none" if dev else "speed" if target == "template_release" else "speed_trace")
    lto = arguments.get("lto", "none")
    if lto == "auto":
        lto = "none" if host == "windows" else "full"
    if lto not in {"none", "full", "thin"}:
        raise ValueError(f"Unsupported lto={lto}")
    if lto == "thin" and not llvm:
        raise ValueError("lto=thin requires use_llvm=yes")
    if optimize not in {"none", "custom", "debug", "speed", "speed_trace", "size"}:
        raise ValueError(f"Unsupported optimize={optimize}")
    kwargs = {"ENV": os.environ.copy()}
    if host == "windows":
        kwargs["TARGET_ARCH"] = {"x86_64": "amd64", "x86_32": "x86", "arm64": "arm64"}[arch]
    env = Environment(**kwargs)
    env.Replace(platform=host, arch=arch, target=target, use_llvm=llvm, use_static_cpp=static,
                debug_crt=debug_crt, debug_symbols=debug, dev_build=dev, optimize=optimize, lto=lto)
    env.Append(CPPPATH=include_paths(native=True) + ["tests"])
    if host == "windows":
        env["is_msvc"] = True
        if llvm:
            env.Replace(CC="clang-cl", CXX="clang-cl")
        env.Append(CXXFLAGS=["/std:c++latest", "/EHsc"])
        runtime = ("/MT" if static else "/MD") + ("d" if debug_crt else "")
        env.Append(CCFLAGS=["/utf-8", runtime])
        env.Append(CPPDEFINES=["NOMINMAX"])
        if debug:
            env.Append(CCFLAGS=["/Z7"], LINKFLAGS=["/DEBUG:FULL"])
        optimization = {"none": "/Od", "debug": "/Od", "size": "/O1", "speed": "/O2", "speed_trace": "/O2"}
        if optimize in optimization:
            env.Append(CCFLAGS=[optimization[optimize]])
        if lto != "none":
            if llvm:
                flag = "-flto=thin" if lto == "thin" else "-flto"
                env.Replace(LINK="lld-link")
                env.Append(CCFLAGS=[flag])
            else:
                env.Append(CCFLAGS=["/GL"], LINKFLAGS=["/LTCG"])
    else:
        if llvm:
            env.Replace(CC="clang", CXX="clang++", LINK="clang++")
        env.Append(CXXFLAGS=["-std=c++23", "-fexceptions"], CCFLAGS=["-pthread"], LINKFLAGS=["-pthread"])
        if arch in {"x86_64", "x86_32"}:
            flag = "-m64" if arch == "x86_64" else "-m32"
            env.Append(CCFLAGS=[flag], LINKFLAGS=[flag])
        if debug:
            env.Append(CCFLAGS=["-g"])
        optimization = {"none": "-O0", "debug": "-Og", "size": "-Os", "speed": "-O3", "speed_trace": "-O2"}
        if optimize in optimization:
            env.Append(CCFLAGS=[optimization[optimize]])
        if static and host == "linux":
            env.Append(LINKFLAGS=["-static-libgcc", "-static-libstdc++"])
        if lto != "none":
            flag = "-flto=thin" if lto == "thin" else "-flto"
            env.Append(CCFLAGS=[flag], LINKFLAGS=[flag])
    if not dev:
        env.Append(CPPDEFINES=["NDEBUG"])
    if not boolean(arguments.get("verbose", "no")):
        env.Replace(CXXCOMSTR="Compiling native $SOURCE", LINKCOMSTR="Linking $TARGET")
    cache = prepare_cache_parent()
    if cache:
        env.CacheDir(cache)
        env.Decider("MD5")
    return env


def disable_compilation_database_emitters(env):
    from SCons.Builder import ListEmitter

    def without_database(emitter):
        if isinstance(emitter, ListEmitter):
            return ListEmitter([
                without_database(child) for child in emitter
                if getattr(child, "__name__", "") != "emit_compilation_db_entry"
            ])
        return emitter

    # Upstream installs the SCons compilation_db tool unconditionally. Its
    # emitters allocate an AlwaysBuild value/builder for every object even when
    # nobody asks for the database. Keep all compiler/Web dependency emitters.
    for name in ("StaticObject", "SharedObject"):
        builder = env["BUILDERS"][name]
        for suffix, emitter in list(builder.emitter.items()):
            builder.emitter[suffix] = without_database(emitter)


def extension_environment(arguments, compilation_database=False):
    from SCons.Builder import ListEmitter
    from SCons.Script import Environment, SConscript
    validate_extension_options(arguments)
    prepare_cache_parent()
    web = arguments.get("platform") == "web"
    env = Environment(tools=["default"], PLATFORM="") if web else Environment()
    env["ENV"] = os.environ.copy()
    if web:
        sys.path.insert(0, str(Path("../scripts").resolve()))
        from web_toolchain import PIN_FILE, load_toolchain, verify_emscripten
        verify_emscripten(load_toolchain())
        if boolean(arguments.get("threads", "no")):
            raise ValueError("The Web export preset requires threads=no.")
        env["threads"] = False

        def depend_on_pin(target, source, env):
            env.Depends(target, str(PIN_FILE))
            return target, source

        for name in ("StaticObject", "SharedObject"):
            builder = env["BUILDERS"][name]
            builder.add_emitter(".cpp", ListEmitter([builder.emitter[".cpp"], depend_on_pin]))

    # Let upstream own options, bindings and C++17 settings, but construct its
    # archive in a toolchain-specific directory: upstream's suffix omits CRT/ABI.
    # Keeping generated bindings at their original path also avoids SCons'
    # generated-header/source-node ambiguity with a whole-tree VariantDir.
    requested_library = arguments.get("build_library")
    arguments["build_library"] = "no"
    try:
        godot_env = SConscript("../godot-cpp/SConstruct", exports={"env": env})
    finally:
        if requested_library is None:
            arguments.pop("build_library")
        else:
            arguments["build_library"] = requested_library
    if not compilation_database:
        disable_compilation_database_emitters(godot_env)
    if godot_env.get("is_msvc", False) and godot_env["debug_symbols"]:
        # Embed compiler symbols in objects rather than sharing a process-CWD
        # compiler PDB between variants; linked PDBs are explicit target outputs.
        godot_env["CCFLAGS"] = remove_flags(godot_env["CCFLAGS"], {"/Zi", "/FS"})
        godot_env.AppendUnique(CCFLAGS=["/Z7"])
    if requested_library is None or boolean(requested_library):
        old_libraries = godot_env["LIBS"]
        godot_env["LIBS"] = []
        root = variant_identity(godot_env, "godot-cpp")
        vendor_sources = []
        for directory in ("src", "src/classes", "src/core", "src/variant"):
            vendor_sources.extend(godot_env.Glob("../godot-cpp/" + directory + "/*.cpp"))
        # Reuse the binding builder's declared outputs, not a filesystem glob:
        # a trimmed API must not pick up stale generated classes from a full API.
        binding_anchor = godot_env.File("../godot-cpp/gen/include/godot_cpp/core/ext_wrappers.gen.inc")
        vendor_sources.extend(node for node in binding_anchor.get_executor().get_all_targets()
                              if str(node).endswith(".cpp"))
        # Binding outputs are not ordered consistently between SCons processes.
        vendor_sources.sort(key=lambda node: node.abspath)
        vendor_root = Path("../godot-cpp").resolve()
        vendor_objects = [
            godot_env.StaticObject(
                target=str(root / "obj" / Path(node.abspath).relative_to(vendor_root).with_suffix("")),
                source=node,
            )[0] for node in vendor_sources
        ]
        if godot_env["platform"] == "linux":
            # Variant paths exceed Linux's single shell-argument limit for this archive.
            godot_env["DEFN_ARCOM"] = godot_env["ARCOM"]
            godot_env["ARCOM"] = "${TEMPFILE(DEFN_ARCOM)}"
        archive = godot_env.StaticLibrary(
            target=str(root / "lib" / ("libgodot-cpp" + godot_env["suffix"] + godot_env["LIBSUFFIX"])),
            source=vendor_objects,
        )
        godot_env["LIBS"] = old_libraries[:-1] + [archive[0]]
    return configure_project(godot_env.Clone())
