# Build system

Run SCons from this directory using the [build environment](../BUILD_ENV.md).
It builds the C++23 extension through godot-cpp and supports Windows, Linux and
the [pinned Web toolchain](../scripts/build_web.py). Native tests build independently
of Godot.

## Common commands

| Command | Purpose |
| --- | --- |
| `scons` | Build and stage the extension for Godot |
| `scons with_hosted_tests=yes` | Build a test-enabled extension without launching Godot |
| `scons unit_tests` | Build the native test executable |
| `scons test` | Build and run native tests |
| `scons test_all` | Run native, hosted, conformance and packaging checks |
| `scons tidy` | Run incremental clang-tidy checks |
| `scons with_tidy=yes test_all` | Run the full suite with clang-tidy |
| `scons coverage` | Run native and hosted coverage and produce a merged report |
| `scons compiledb` | Generate the compilation database |
| `scons packaging` | Check exported asset coverage without building |

Add `with_tidy=yes` to an extension build to also run clang-tidy.
Use `target=template_release` for release builds. Godot-driven checks accept
`godot_bin=<path>` or `GODOT_BIN`. They share one project import per invocation
and run with isolated user data.

Individual checks are available through `hosted_test`, `conformance`,
`coverage_native` and `coverage_hosted`. Measurement runners are `sim`, `balance`,
`matrix` and `endless`; their options are defined in
[the runner registry](build_tools/runners.py).

## Build outputs

Incompatible configurations keep separate artifacts:

```text
build/<platform>-<arch>/<target>/<compiler>-<configuration hash>/<kind>/
```

The selected binaries are staged in `bin` for Godot and export tooling.
Coverage writes `summary.txt` and `html/index.html` under
`build/coverage/<suite>`, where `<suite>` is `native`, `hosted` or `merged`.
CI caches compiler outputs across compatible builds.

Parallel jobs within one SCons invocation are supported; avoid separate builds
that simultaneously stage different configurations into the same `bin` directory.

## Packaging and releases

Export presets ship only assets listed in `export_files`. `scons packaging`
reports missing entries and runs as part of `test_all`, without needing Godot.
From the repository root, `python scripts/check_export_presets.py --stale` also
reports listed assets that nothing reaches.

Create a native release archive from the repository root with
`python scripts/build.py --platform windows` or
`python scripts/build.py --platform linux`.

For implementation details, see [SConstruct](SConstruct) and
[build_tools](build_tools/).
