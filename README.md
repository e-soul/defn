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

The build should be invoked from within the `defn` directory, where `SConstruct` is, not the repo root.

Build the extension only: `scons`

Build with clang-tidy and run all tests: `scons with_tidy=True test_all`

See the [build system overview](defn/BUILD_SYSTEM.md) for details.
