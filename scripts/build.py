from __future__ import annotations

import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys
import time
import urllib.request
import zipfile
from pathlib import Path


DEFAULT_GODOT_VERSION = "4.7"
DEFAULT_BUILD_DIR: str = "build"
DEFAULT_ARCHIVE_NAME: str = "defn-win-x86_64.zip"
DEFAULT_BUILD_VERSION: str = "snapshot"
GODOT_EXECUTABLE_ENV_VAR = "GODOT_BIN"
DEFAULT_EXPORT_PRESET_NAME: str = "defn_win_release_x86_64"


def format_command(command: list[str]) -> str:
    if os.name == "nt":
        return subprocess.list2cmdline(command)

    return shlex.join(command)


def run_command(command: list[str], cwd: Path | None = None) -> None:
    print(f"Running: {format_command(command)}")
    subprocess.run(command, check=True, cwd=cwd)


def resolve_existing_path(configured_path: str, description: str) -> Path:
    path = Path(configured_path).expanduser().resolve()
    if not path.exists():
        raise FileNotFoundError(
            f"Configured {description} does not exist: {path}"
        )

    return path


def sanitize_build_version(build_version: str) -> str:
    sanitized = re.sub(r"[^A-Za-z0-9._-]+", "-", build_version.strip())
    sanitized = sanitized.strip("-._")
    if sanitized:
        return sanitized

    return DEFAULT_BUILD_VERSION


def versioned_archive_name(archive_name: str, build_version: str) -> str:
    archive_path = Path(archive_name)
    archive_stem = archive_path.stem
    archive_suffix = archive_path.suffix or ".zip"
    sanitized_build_version = sanitize_build_version(build_version)
    return f"{archive_stem}-{sanitized_build_version}{archive_suffix}"


def download_file(url: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    print(f"Downloading: {url}")
    request = urllib.request.Request(
        url,
        headers={"User-Agent": "defn-build-bot/1.0"},
    )
    with urllib.request.urlopen(request) as response:
        with destination.open("wb") as file_handle:
            shutil.copyfileobj(response, file_handle)


def extract_zip(archive_path: Path, destination: Path) -> None:
    if destination.exists():
        shutil.rmtree(destination)

    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive_path) as archive:
        archive.extractall(destination)


def copy_tree(source: Path, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    for item in source.iterdir():
        target = destination / item.name
        if item.is_dir():
            shutil.copytree(item, target, dirs_exist_ok=True)
            continue

        shutil.copy2(item, target)


def find_godot_executable(directory: Path) -> Path:
    console_executables = sorted(directory.rglob("Godot*_console.exe"))
    if console_executables:
        return console_executables[0]

    executables = sorted(directory.rglob("Godot*.exe"))
    if not executables:
        raise FileNotFoundError(
            f"Unable to locate a Godot executable under {directory}"
        )

    return executables[0]


def templates_install_dir(version: str) -> Path:
    appdata = os.environ.get("APPDATA", "").strip()
    if not appdata:
        raise EnvironmentError(
            "APPDATA is required to install Godot export templates on Windows."
        )

    return Path(appdata) / "Godot" / "export_templates" / f"{version}.stable"


def ensure_godot_export_templates(version: str, output_dir: Path) -> None:
    cache_dir = output_dir / ".cache" / "godot" / f"{version}.stable"
    templates_archive = cache_dir / "export_templates.tpz"
    templates_dir = cache_dir / "templates"
    installed_templates_dir = templates_install_dir(version)
    templates_url = (
        "https://downloads.godotengine.org/"
        f"?version={version}&flavor=stable&slug=export_templates.tpz"
        "&platform=templates"
    )

    templates_installed = (installed_templates_dir / "version.txt").exists()
    if not templates_installed:
        if not templates_archive.exists():
            download_file(templates_url, templates_archive)
        extract_zip(templates_archive, templates_dir)
        source_dir = templates_dir / "templates"
        if not source_dir.exists():
            source_dir = templates_dir
        copy_tree(source_dir, installed_templates_dir)


def ensure_godot(version: str, output_dir: Path) -> str:
    cache_dir = output_dir / ".cache" / "godot" / f"{version}.stable"
    editor_archive = cache_dir / "editor.zip"
    editor_dir = cache_dir / "editor"
    editor_url = (
        "https://downloads.godotengine.org/"
        f"?version={version}&flavor=stable&slug=win64.exe.zip"
        "&platform=windows.64"
    )

    try:
        godot_executable = find_godot_executable(editor_dir)
    except FileNotFoundError:
        if not editor_archive.exists():
            download_file(editor_url, editor_archive)
        extract_zip(editor_archive, editor_dir)
        godot_executable = find_godot_executable(editor_dir)

    ensure_godot_export_templates(version, output_dir)

    return str(godot_executable)


def resolve_godot_executable(
    configured_path: str | None,
    version: str,
    output_dir: Path,
) -> str:
    if configured_path:
        godot_executable = resolve_existing_path(
            configured_path,
            "Godot executable",
        )
        ensure_godot_export_templates(version, output_dir)
        return str(godot_executable)

    env_godot = os.environ.get(GODOT_EXECUTABLE_ENV_VAR, "").strip()
    if env_godot:
        godot_executable = resolve_existing_path(env_godot, "Godot executable")
        ensure_godot_export_templates(version, output_dir)
        return str(godot_executable)

    return ensure_godot(version, output_dir)


def write_github_env(name: str, value: str) -> None:
    github_env_path = os.environ.get("GITHUB_ENV", "").strip()
    if not github_env_path:
        return

    with Path(github_env_path).open("a", encoding="utf-8") as env_file:
        env_file.write(f"{name}={value}\n")


def wait_for_exported_files(
    directory: Path,
    timeout_seconds: float = 60.0,
    poll_interval_seconds: float = 0.5,
) -> list[Path]:
    deadline = time.monotonic() + timeout_seconds

    while time.monotonic() < deadline:
        files = sorted(path for path in directory.rglob("*") if path.is_file())
        if files:
            return files

        time.sleep(poll_interval_seconds)

    return sorted(path for path in directory.rglob("*") if path.is_file())


def create_archive(source_dir: Path, archive_path: Path) -> None:
    archive_path.parent.mkdir(parents=True, exist_ok=True)
    base_name = archive_path.with_suffix("")
    shutil.make_archive(str(base_name), "zip", root_dir=source_dir)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build and export the Windows release headlessly."
    )
    parser.add_argument(
        "--godot-exe",
        default=None,
        help="Path to the Godot editor executable.",
    )
    parser.add_argument(
        "--godot-version",
        default=DEFAULT_GODOT_VERSION,
        help="Godot version to download when no executable is configured.",
    )
    parser.add_argument(
        "--output-dir",
        default=None,
        help="Directory that receives export outputs and the ZIP archive.",
    )
    parser.add_argument(
        "--archive-name",
        default=DEFAULT_ARCHIVE_NAME,
        help="Name of the ZIP archive to produce.",
    )
    parser.add_argument(
        "--build-version",
        default=DEFAULT_BUILD_VERSION,
        help="Version label appended to the export directory and archive.",
    )
    parser.add_argument(
        "--export-preset",
        default=DEFAULT_EXPORT_PRESET_NAME,
        help="Godot export preset name to use.",
    )
    parser.add_argument(
        "--skip-archive",
        action="store_true",
        help="Skip ZIP creation and leave the exported files unpacked.",
    )
    parser.add_argument(
        "--setup-godot-env",
        action="store_true",
        help="Resolve or download Godot and write GODOT_BIN to GITHUB_ENV.",
    )
    parser.add_argument(
        "--print-godot-version",
        action="store_true",
        help="Print the configured Godot version and exit.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.print_godot_version:
        print(DEFAULT_GODOT_VERSION)
        return 0

    repo_root = Path(__file__).resolve().parent.parent
    project_dir = repo_root / "defn"

    if args.output_dir:
        output_dir = Path(args.output_dir).expanduser().resolve()
    else:
        output_dir = repo_root / DEFAULT_BUILD_DIR

    if args.setup_godot_env:
        godot_executable = resolve_godot_executable(
            args.godot_exe,
            args.godot_version,
            output_dir,
        )
        os.environ[GODOT_EXECUTABLE_ENV_VAR] = godot_executable
        write_github_env(GODOT_EXECUTABLE_ENV_VAR, godot_executable)
        print(f"{GODOT_EXECUTABLE_ENV_VAR}={godot_executable}")
        return 0

    archive_name = versioned_archive_name(
        args.archive_name,
        args.build_version,
    )
    export_dir = output_dir / Path(archive_name).stem
    archive_path = output_dir / archive_name
    export_exe_path = export_dir / "defn.exe"
    project_dll_path = (
        project_dir / "bin" / "defn_core.windows.template_release.x86_64.dll"
    )
    godot_executable = resolve_godot_executable(
        args.godot_exe,
        args.godot_version,
        output_dir,
    )

    if export_dir.exists():
        shutil.rmtree(export_dir)

    if archive_path.exists():
        archive_path.unlink()

    export_dir.mkdir(parents=True, exist_ok=True)

    run_command(
        [
            sys.executable,
            "-m",
            "SCons",
            "platform=windows",
            "target=template_release",
        ],
        cwd=project_dir,
    )

    if not project_dll_path.exists():
        raise FileNotFoundError(
            f"Expected release DLL was not produced: {project_dll_path}"
        )

    run_command(
        [
            godot_executable,
            "--headless",
            "--path",
            str(project_dir),
            "--import",
        ]
    )
    run_command(
        [
            godot_executable,
            "--headless",
            "--path",
            str(project_dir),
            "--export-release",
            args.export_preset,
            str(export_exe_path),
        ]
    )

    exported_files = wait_for_exported_files(export_dir)
    if not exported_files:
        raise FileNotFoundError(
            f"Godot export did not produce any files under {export_dir}"
        )

    if not args.skip_archive:
        create_archive(export_dir, archive_path)

        if not archive_path.exists():
            raise FileNotFoundError(
                f"Expected archive was not created: {archive_path}"
            )

    print("Windows release export complete.")
    print(f"Build version: {sanitize_build_version(args.build_version)}")
    print(f"Export directory: {export_dir}")
    if not args.skip_archive:
        print(f"Archive: {archive_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
