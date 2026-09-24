# Web build

Defn's gameplay is a C++ GDExtension. Exporting from the editor packages that
extension; it does not compile a native library into WebAssembly.

## Prerequisites

- Python 3.10+ and SCons available in your shell (a virtual environment is recommended).
- Git, the repository's submodules and Git LFS assets.
- A native C++ build environment for your host, just as for a desktop build.
  Automatic exports support Windows and Linux; the editor needs a native debug
  extension to import the game's classes.

Commands use `python`; use `python3` if that is how your installation is named.
The wrapper can be run from any working directory.

## Set up once

[web-toolchain.json](../scripts/web-toolchain.json) is the Web toolchain pin: Emscripten
4.0.20, the exact emsdk manager revision, and Godot 4.7.2. The setup command
installs and activates that SDK in the ignored `build/emsdk` directory:

```text
python scripts/build_web.py --setup
```

This is a local install, not a global one. Setup is explicit; builds never
silently install or switch Emscripten versions. Re-running setup is safe for the
pinned checkout. An existing SDK at a different Git revision is rejected rather
than reset. To keep another SDK alongside it, pass `--emsdk-dir <directory>` to
both setup and subsequent build commands.

## Build and export

From the repository root, use the same command locally and in CI:

```text
python scripts/build_web.py --target both
```

The wrapper:

1. Checks the SDK revision and actual compiler version, then selects its paths
   and configuration for child processes only. No shell activation is needed.
2. Resolves Godot from `--godot-exe`, `GODOT_BIN`, or a download of the pinned
   editor. A configured editor with a different version is rejected.
3. Installs matching export templates when absent and builds the native debug
   extension needed by the editor.
4. Builds the requested Web libraries, checks packaging, imports resources and
   exports through the `defn_web_release` preset.
5. Validates fresh export files before copying them to `build/web/debug` and
   `build/web/release`. Godot diagnostics are checked even when its exit code is
   zero. Existing unrelated files in the output directories are not removed.

The default target is `release`; `--target debug` builds only the debug export.
Use `--jobs N` to control compilation parallelism, `--output-dir <directory>` to
change the export root, or `--build-only` to compile WASM without invoking Godot.
Godot downloads are cached in `build/.cache/godot`.

Web builds default to `threads=no`, matching the Web preset and the library
entries in [defn_core.gdextension](defn_core.gdextension). Outputs:

- `bin/libdefn_core.web.template_release.wasm32.nothreads.wasm`
- `bin/libdefn_core.web.template_debug.wasm32.nothreads.wasm`

Native builds remain unchanged. Web objects use a separate directory. Web
compilation disables C++ exceptions to match the stock Godot templates, which
do not export the C++ exception tag; shipped game code has no exception handlers.

Direct SCons Web builds also verify the actual `emcc --version` against the pin
and fail on a mismatch; they require an already activated SDK. Changing `PATH`
to a newer compiler cannot silently bypass the check. Web objects, including
godot-cpp, depend on the pin so an upgrade invalidates cached compilation.

## Serve

From the repository root:

```text
python -m http.server 8000 --bind 127.0.0.1 --directory build/web/release
```

Open **http://127.0.0.1:8000/index.html**, not a `file://` URL. Any static HTTP
server can be used instead. Keep all generated files together, including the
extension WASM and engine side module; do not rename individual exported files.
For editor exports, use the **defn_web_release** preset after building its Web
library. Select **Export With Debug** only for a built debug library. The editor
does not compile the extension or enforce the wrapper's Godot version check.

The single-threaded 4.7.2 export works without cross-origin isolation headers or
a service worker. PWA remains disabled. Threaded Web builds are not configured;
enabling threads would also require matching extension binaries and server-side
cross-origin isolation. Use HTTPS for production hosting.

After replacing an export, clear cached site data if the browser still runs an
old build, including any service worker left by an earlier PWA export.

## Custom page

[export_templates/web_shell.html](export_templates/web_shell.html) is the Web
preset's HTML shell. Edit that source, then re-export; editing generated HTML
will be overwritten. Godot replaces its engine URL, configuration and head
placeholders during export, so the shell also works with renamed export targets.

The shell provides a responsive game viewport, branded download progress,
startup errors with a retry action, and an optional fullscreen control. It uses
system fonts and inline styles/scripts. The Google tag is the shell's only
external dependency and records web deployment analytics. Its CSS tokens mirror the dark neutrals and accent in
[data/ui_theme.json](data/ui_theme.json); keep them aligned when changing the
game palette. Only the browser shell uses JavaScript; gameplay stays in C++.

## Tests and CI

From the repository root, check the wrapper, packaging and shell behavior
(Node.js 18 or newer is required only for shell tests):

```text
python -m unittest discover -s scripts/tests -p "test_web*.py"
python scripts/check_export_presets.py
node --test defn/tests/web_shell.test.cjs
```

To test actual exported WASM in Chromium:

```text
python -m pip install -r scripts/requirements-web-test.txt
python -m playwright install chromium
python scripts/smoke_web.py build/web/debug build/web/release
```

Linux CI uses `python -m playwright install --with-deps chromium` to install
browser system dependencies too. The smoke test serves each export on a
temporary loopback port, checks engine/compiler and content-validation logs,
rejects browser/network errors, and checks responsive viewport sizing.

[Web CI](../.github/workflows/web-ci.yml) runs on pushes to `master` and manual
dispatch, matching the existing native workflows. It uses the same private
asset checkout, setup and build wrapper; tests both debug and release exports;
and uploads playable exports as an artifact. Standalone Web CI runs do not
deploy a website. SDK and compiler caches are keyed by the toolchain pin.
Browser checks serve exports below a URL prefix, matching a Pages project site.

## GitHub Pages

[Release](../.github/workflows/release.yml) calls the same Web CI workflow and
uploads the tested `build/web/release` directory with
`actions/upload-pages-artifact`. Once the native and Web builds succeed, a
separate job deploys that artifact with `actions/deploy-pages`. No generated
files are committed and no `gh-pages` branch is needed.

Both `v*` tag pushes and manual **Release** runs deploy the selected ref's Web
build to **https://e-soul.github.io/defn/**. Ordinary `master` pushes and manual
**Web CI** runs only build and test. To deploy only the Web build, open
**Actions > Deploy Web to Pages > Run workflow** and select the source branch
from the **Branch** dropdown (the repository default is `master`). The manual
workflow reuses Web CI's pinned toolchain, build caches, tests, and release
export; it deploys only after the build and browser checks succeed. From the
CLI, run `gh workflow run deploy-web.yml --ref BRANCH`, omitting `--ref` for
`master`. Native release ZIP publishing remains tag-only. Pages deployments
are serialized without cancelling an active deploy; the `github-pages`
environment records the deployed URL.

One-time repository setup (requires administrator access):

1. In **Settings > Pages > Build and deployment**, set **Source** to
   **GitHub Actions**, not a branch.
2. If the `github-pages` environment restricts deployment refs, allow release
   tags matching `v*` and any branches used for manual Release or **Deploy Web
   to Pages** runs.
3. Push a release tag or run **Release** or **Deploy Web to Pages** after these
   workflow changes are on the selected ref. The site becomes available after
   its first deployment.

The Pages artifact contains the complete release export at its root, including
`index.html`, the PCK, engine WASM and extension WASM. Relative asset URLs work
under `/defn/`; the single-threaded export needs no custom HTTP headers.

## Toolchain upgrades

When upgrading, change the Emscripten/Godot pairing and emsdk revision in the pin
together, use a separate SDK directory if the manager revision changes, and run
the complete build/browser checks. Keep the native default Godot version in
[scripts/build.py](../scripts/build.py) aligned. Do not change only the extension
compiler to silence a browser warning.

## Console diagnostics

- **No GDExtension support**: Extensions Support was disabled when exporting.
- **Cannot get class 'SettingsRuntime'**: the extension did not load; inspect
  earlier errors and verify the matching Web library was exported.
- **Cannot open file 'res://scenes/menu.tscn'**: required resources are missing,
  or the browser is serving a stale export. The packaging check covers the
  scenes, JSON content and assets loaded by C++.
- **AudioContext prevented from starting automatically**: click the game to
  allow audio; this warning does not prevent the menu from loading.
- **Deprecated WebAssembly `try` instruction**: a toolchain/template warning,
  not a missing-class or missing-scene failure. Keep the compiler compatible
  with the template rather than upgrading only one side to silence it.
