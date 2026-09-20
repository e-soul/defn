"""Central target selection: optional environments and graphs are genuinely lazy."""

from dataclasses import dataclass
from .runners import RUNNERS

PUBLIC_TARGETS = {
    "extension", "unit_tests", "test", "test_all", "tidy", "packaging",
    "coverage_native", "coverage_hosted", "coverage", "compiledb", "compilation_db",
    "compile_commands.json", *RUNNERS,
}


@dataclass(frozen=True)
class Selection:
    extension: bool
    native: bool
    native_coverage: bool
    hosted_coverage: bool
    hosted: bool
    packaging: bool
    tidy: bool
    database: bool
    runners: frozenset


def select(targets, hosted=False, tidy=False, compiledb=False):
    targets = set(targets)
    unknown = targets - PUBLIC_TARGETS
    if unknown:
        raise ValueError("Unsupported build targets: " + ", ".join(sorted(unknown))
                         + ". Use a public alias (see BUILD_SYSTEM.md).")
    runners = targets.intersection(RUNNERS)
    if "test_all" in targets:
        runners.update({"hosted_test", "conformance"})
    native = bool(targets.intersection({"unit_tests", "test", "test_all"}))
    native_coverage = bool(targets.intersection({"coverage_native", "coverage"}))
    hosted_coverage = bool(targets.intersection({"coverage_hosted", "coverage"}))
    database = compiledb or bool(targets.intersection({"compiledb", "compilation_db", "compile_commands.json"}))
    extension = (not targets or bool(targets.intersection({"extension", "tidy"}))
                 or bool(runners) or hosted_coverage)
    # Native + compiledb describes native, not an implicitly requested extension.
    if database and not (native or native_coverage):
        extension = True
    if tidy and not extension:
        raise ValueError("with_tidy=yes requires an extension target; use tidy for extension analysis")
    return Selection(extension, native, native_coverage, hosted_coverage,
                     hosted or bool(runners) or hosted_coverage,
                     bool(targets.intersection({"packaging", "test_all"})),
                     tidy or "tidy" in targets, database, frozenset(runners))
