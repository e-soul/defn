![GitHub Windows CI Workflow Status](https://img.shields.io/github/actions/workflow/status/e-soul/defn/windows-ci.yml?label=Windows%20CI)
![GitHub Linux CI Workflow Status](https://img.shields.io/github/actions/workflow/status/e-soul/defn/linux-ci.yml?label=Linux%20CI)
![GitHub Release Workflow Status](https://img.shields.io/github/actions/workflow/status/e-soul/defn/release.yml?label=release%20build)
![coverage](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2Fe-soul%2Fdefn%2Frefs%2Fheads%2Fbuild-artifacts%2Fcoverage.json)
![clang-tidy](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2Fe-soul%2Fdefn%2Frefs%2Fheads%2Fbuild-artifacts%2Fclang-tidy.json)

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/e-soul/defn)

# Defn

A simple belt-scroller tug-of-war 2.5D game. For more details see the [Game Design Document](GDD.md) or the [Target Architecture](ARCH.md).

Powered by ![The Godot Engine](https://img.shields.io/badge/Godot-white?logo=godotengine) and ![The C++ Programming Language](https://img.shields.io/badge/C++-blue?logo=cplusplus)

## Build and test

The build should be invoked from within the `defn` directory, where `SConstruct` is. SCons defaults to the native Windows or Linux target for the host operating system.

Build the normal extension only: `scons`

Build with clang-tidy enabled: `scons with_tidy=yes`

Only run clang-tidy: `scons tidy`

Build the native test executable only: `scons unit_tests`

Build and run the native host-independent suite: `scons test`

Build and run the native host-independent suite with LLVM coverage: `scons coverage_native`

Build and run the Godot-hosted suite with LLVM coverage: `scons coverage_hosted`

Build and run both coverage suites and emit a merged report: `scons coverage`

Coverage writes text summaries to `build/coverage/<suite>/summary.txt` and HTML reports to `build/coverage/<suite>/html/index.html`, where `<suite>` is `native`, `hosted`, or `merged`.

Build a test-enabled extension DLL, but do not launch Godot: `scons with_hosted_tests=yes`

Build and run the broader headless Godot-hosted suite: `scons hosted_test godot_bin=path/to/godot_executable`

Build and run both suites together: `scons test_all godot_bin=path/to/godot_executable`

Check that every asset the game can reach is actually packaged: `scons packaging`. It runs as part of `scons test_all`, needs neither Godot nor the extension, and fails with the paths to add. The presets ship only what `export_files` names, so a new unit renders in a debug run and is missing its sprites in the exported build. Add `--stale` when running `python scripts/check_export_presets.py` directly to also see listed entries nothing reaches.

The hosted suite is launched through godot_hosted_runner.gd and calls into the Godot-exposed C++ runner in `defn_hosted_test_runner.cpp`. You can also use the environment variable `GODOT_BIN` instead of passing godot_bin on the command line.

Re-measure where each animation clip is anchored, after adding a unit or swapping its sprites: `python scripts/gen_anim_offsets.py --report` to read the numbers, `--write` to update `data/unit_data.json`, `--contact-sheet out.png` to eyeball them. The clips are cropped to different canvases, so without these offsets a unit's body jumps when it switches pose.

Create a native release archive from the repository root: `python scripts/build.py --platform windows` or `python scripts/build.py --platform linux`.
