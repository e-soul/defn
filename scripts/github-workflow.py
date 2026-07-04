from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


DEFAULT_COVERAGE_SUMMARY = "defn/build/coverage/merged/summary.txt"
DEFAULT_CLANG_TIDY_REPORT = "defn/build/clang-tidy-report.txt"
DEFAULT_OUTPUT_DIR = "build/artifacts"
NEUTRAL_COLOR = "lightgrey"


def read_text(path: Path) -> str:
    for encoding in ("utf-8-sig", "utf-16"):
        try:
            return path.read_text(encoding=encoding)
        except UnicodeError:
            continue

    return path.read_text(errors="replace")


def coverage_color(coverage_percent: float | None) -> str:
    if coverage_percent is None:
        return NEUTRAL_COLOR
    if coverage_percent >= 80.0:
        return "brightgreen"
    if coverage_percent >= 70.0:
        return "yellowgreen"
    if coverage_percent >= 60.0:
        return "yellow"
    return "red"


def clang_tidy_color(issue_count: int | None) -> str:
    if issue_count is None:
        return NEUTRAL_COLOR
    if issue_count == 0:
        return "brightgreen"
    if issue_count <= 9:
        return "yellow"
    if issue_count <= 49:
        return "orange"
    return "red"


def parse_coverage_percent(summary_text: str) -> float | None:
    for line in summary_text.splitlines():
        if not line.startswith("TOTAL"):
            continue

        percentages = re.findall(r"(\d+(?:\.\d+)?)%", line)
        if len(percentages) < 3:
            return None

        return float(percentages[2])

    return None


def parse_clang_tidy_issue_count(report_text: str) -> int:
    issue_count = 0
    posix_diagnostic = re.compile(
        r"^(?:[A-Za-z]:)?[^\r\n]*?:\d+:\d+:\s+"
        r"(?:warning|error):\s+.*\[[^\]]+\]\s*$"
    )
    msvc_diagnostic = re.compile(
        r"^.*\(\d+,\d+\):\s+(?:warning|error):\s+.*\[[^\]]+\]\s*$"
    )

    for line in report_text.splitlines():
        if posix_diagnostic.match(line) or msvc_diagnostic.match(line):
            issue_count += 1

    return issue_count


def load_coverage_percent(summary_path: Path) -> float | None:
    if not summary_path.exists():
        return None

    try:
        return parse_coverage_percent(read_text(summary_path))
    except OSError:
        return None


def load_clang_tidy_issue_count(report_path: Path) -> int | None:
    if not report_path.exists():
        return None

    try:
        return parse_clang_tidy_issue_count(read_text(report_path))
    except OSError:
        return None


def write_badge(path: Path, label: str, message: str, color: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "schemaVersion": 1,
        "label": label,
        "message": message,
        "color": color,
    }
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def generate_badges(
    coverage_summary: Path,
    clang_tidy_report: Path,
    output_dir: Path,
) -> None:
    coverage_percent = load_coverage_percent(coverage_summary)
    coverage_message = (
        f"{coverage_percent:.2f}%"
        if coverage_percent is not None
        else "unknown"
    )
    write_badge(
        output_dir / "coverage.json",
        "coverage",
        coverage_message,
        coverage_color(coverage_percent),
    )

    issue_count = load_clang_tidy_issue_count(clang_tidy_report)
    issue_message = (
        f"{issue_count} {'issue' if issue_count == 1 else 'issues'}"
        if issue_count is not None
        else "unknown"
    )
    write_badge(
        output_dir / "clang-tidy.json",
        "clang-tidy",
        issue_message,
        clang_tidy_color(issue_count),
    )

    print(f"Wrote badge artifacts to {output_dir}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="GitHub Actions helper for generated workflow artifacts."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    badges = subparsers.add_parser(
        "badges",
        help="Generate Shields endpoint JSON from CI build outputs.",
    )
    badges.add_argument(
        "--coverage-summary",
        default=DEFAULT_COVERAGE_SUMMARY,
        help="Path to the merged coverage summary text report.",
    )
    badges.add_argument(
        "--clang-tidy-report",
        default=DEFAULT_CLANG_TIDY_REPORT,
        help="Path to the clang-tidy report text file.",
    )
    badges.add_argument(
        "--output-dir",
        default=DEFAULT_OUTPUT_DIR,
        help="Directory that receives generated badge JSON files.",
    )

    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parent.parent

    if args.command == "badges":
        generate_badges(
            repo_root / args.coverage_summary,
            repo_root / args.clang_tidy_report,
            repo_root / args.output_dir,
        )
        return 0

    return 1


if __name__ == "__main__":
    raise SystemExit(main())
