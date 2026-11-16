#!/usr/bin/env python3
"""Checks that each engine module only includes headers of its own module or of the modules it depends on.

Every target exports `engine/src` as an include directory, so any module can include any header: only the linker
notices, and not for header-only code. This script rebuilds the boundaries from the build itself (`xmake l
scripts/xmake/dump_targets.lua`): a header belongs to the engine target whose sources live in the deepest directory
above it, and a file may include the headers of its own target and of the targets it depends on (`add_deps`,
followed transitively, as the linker does).

Usage (from the repository root, after `xmake f`, graphics on so every module is configured):
    python3 scripts/check_module_boundaries.py            # report only
    python3 scripts/check_module_boundaries.py --strict   # exit 1 on any violation (CI)
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "engine" / "src"
# Header-only targets have no source file to locate them: their directory is given here.
HEADER_ONLY_DIRS = {"engine_events": "events", "engine_render_api": "render/api"}
# Aggregates, not modules.
IGNORED_TARGETS = {"engine_headers", "engine_all"}
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.MULTILINE)


def load_targets() -> dict:
    """Runs xmake to get every target with its kind, deps and source files."""
    result = subprocess.run(["xmake", "l", "scripts/xmake/dump_targets.lua"], cwd=ROOT, capture_output=True,
                            text=True, check=True)
    start = result.stdout.index("{")
    return json.loads(result.stdout[start:])


def engine_targets(targets: dict) -> dict[str, dict]:
    """Keeps the engine modules."""
    return {name: info for name, info in targets.items()
            if name.startswith("engine_") and name not in IGNORED_TARGETS}


def directory_owners(modules: dict[str, dict]) -> dict[PurePosixPath, str]:
    """Maps each directory holding sources of a module (relative to engine/src) to that module."""
    owners: dict[PurePosixPath, str] = {}
    for name, info in modules.items():
        dirs = {PurePosixPath(f).relative_to("engine/src").parent for f in info["files"] or []}
        if name in HEADER_ONLY_DIRS:
            dirs.add(PurePosixPath(HEADER_ONLY_DIRS[name]))
        for directory in dirs:
            previous = owners.setdefault(directory, name)
            if previous != name:
                raise SystemExit(f"error: {directory}/ holds sources of both {previous} and {name}")
    return owners


def owner_of(path: PurePosixPath, owners: dict[PurePosixPath, str]) -> str | None:
    """Module of a file: the owner of the deepest directory above it."""
    for directory in [path.parent, *path.parent.parents]:
        if directory in owners:
            return owners[directory]
    return None


def closure(name: str, modules: dict[str, dict]) -> set[str]:
    """A module and every module it depends on, transitively."""
    seen: set[str] = set()
    stack = [name]
    while stack:
        current = stack.pop()
        if current in seen or current not in modules:
            continue
        seen.add(current)
        stack.extend(modules[current]["deps"] or [])
    return seen


def resolve(include: str, including: Path) -> Path | None:
    """The engine header an include names, if any: relative to engine/src, or to the including file."""
    for base in (SRC, including.parent):
        candidate = (base / include).resolve()
        if candidate.is_file() and SRC in candidate.parents:
            return candidate
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--strict", action="store_true", help="exit with 1 if a boundary is crossed")
    args = parser.parse_args()

    modules = engine_targets(load_targets())
    owners = directory_owners(modules)
    allowed = {name: closure(name, modules) for name in modules}

    violations: dict[str, list[str]] = defaultdict(list)
    unowned: set[str] = set()
    for file in sorted(SRC.rglob("*")):
        if file.suffix not in {".cpp", ".hpp", ".tpp"}:
            continue
        relative = PurePosixPath(file.relative_to(SRC).as_posix())
        module = owner_of(relative, owners)
        if module is None:
            unowned.add(str(relative))
            continue
        for include in INCLUDE.findall(file.read_text(encoding="utf-8", errors="replace")):
            header = resolve(include, file)
            if header is None:
                continue
            header_relative = PurePosixPath(header.relative_to(SRC).as_posix())
            header_module = owner_of(header_relative, owners)
            if header_module is None:
                violations[module].append(f"{relative}: {include} (belongs to no module)")
            elif header_module not in allowed[module]:
                violations[module].append(f"{relative}: {include} ({header_module}, not a dependency)")

    for module in sorted(violations):
        print(f"{module}:")
        for line in violations[module]:
            print(f"  {line}")
    if unowned:
        print("files in no module:")
        for path in sorted(unowned):
            print(f"  {path}")
    count = sum(len(lines) for lines in violations.values()) + len(unowned)
    print(f"{count} boundary violation(s) in {len(modules)} modules")
    return 1 if args.strict and count > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
