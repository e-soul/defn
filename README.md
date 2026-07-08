![GitHub Release](https://img.shields.io/github/v/release/e-soul/defn)

![GitHub CI Workflow Status](https://img.shields.io/github/actions/workflow/status/e-soul/defn/windows-ci.yml?label=CI%20build)
![GitHub Release Workflow Status](https://img.shields.io/github/actions/workflow/status/e-soul/defn/windows-release.yml?label=release%20build)
![coverage](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2Fe-soul%2Fdefn%2Frefs%2Fheads%2Fbuild-artifacts%2Fcoverage.json)
![clang-tidy](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2Fe-soul%2Fdefn%2Frefs%2Fheads%2Fbuild-artifacts%2Fclang-tidy.json)

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/e-soul/defn)

# Defn

A simple belt-scroller tug-of-war 2.5D game. See the [Game Design Document](GDD.md) for details.

Powered by ![The Godot Engine](https://img.shields.io/badge/Godot-white?logo=godotengine) and ![The C++ Programming Language](https://img.shields.io/badge/C++-blue?logo=cplusplus)

## Build and test

The build should be invoked from within the "defn" directory, where `SConstruct` is.

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

The hosted suite is launched through godot_hosted_runner.gd and calls into the Godot-exposed C++ runner in `defn_hosted_test_runner.cpp`. You can also use the environment variable `GODOT_BIN` instead of passing godot_bin on the command line.
