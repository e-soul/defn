# Defn

## Build and test

The build should be invoked from within the "defn" directory, where `SConstruct` is.

Build the normal extension only: `scons`

Build with clang-tidy enabled: `scons with_tidy=yes`

Only run clang-tidy: `scons tidy`

Build the native test executable only: `scons unit_tests`

Build and run the native host-independent suite: `scons test`

Build a test-enabled extension DLL, but do not launch Godot: `scons with_hosted_tests=yes`

Build and run the broader headless Godot-hosted suite: `scons hosted_test godot_bin=path/to/godot_executable`

Build and run both suites together: `scons test_all godot_bin=path/to/godot_executable`

The hosted suite is launched through godot_hosted_runner.gd and calls into the Godot-exposed C++ runner in `defn_hosted_test_runner.cpp`. You can also use the environment variable `GODOT_BIN` instead of passing godot_bin on the command line.
