#!/usr/bin/env python3
"""Mechanized #line discipline gate for the BMK4_ORACLE1 engine hooks.

The shipping preprocessor's view of an instrumented file is obtained by
stripping every `#ifdef BMK4_ORACLE1 ... #endif` block and every `#line`
directive (which exist only because of those insertions). This gate
verifies, without needing a pre-edit baseline, that every `#line N`
directive restores the numbering exactly: N must equal 1 + the number of
shipping-view lines that precede it, so `__LINE__` (and therefore every
iassert/vassert string literal in Debug shipping builds) is identical
whether or not BMK4_ORACLE1 is defined.

Optionally, --baseline-rev <rev> additionally byte-compares the shipping
view against `git show <rev>:<path>` (used at desk, where a pre-edit
baseline exists; CI runs the structural mode).

Exit 0 = green; exit 1 = red with reasons.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

INSTRUMENTED = [
    "src/database/db_stream.cpp",
    "src/database/db_stream_load.cpp",
    "src/database/db_file_load.cpp",
    "src/database/db_load.cpp",
    "src/database/db_stringtable_load.cpp",
    "src/database/db_registry.cpp",
]


def shipping_view(lines: list[str]) -> tuple[list[str], list[tuple[int, int]], list[str]]:
    """Returns (shipping_lines, [(line_directive_value, shipping_index)], problems)."""
    shipping: list[str] = []
    checks: list[tuple[int, int]] = []
    problems: list[str] = []
    in_guard = False
    for physical, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped == "#ifdef BMK4_ORACLE1":
            if in_guard:
                problems.append(f"nested BMK4_ORACLE1 guard at physical line {physical}")
            in_guard = True
            continue
        if in_guard:
            if stripped == "#endif":
                in_guard = False
            elif stripped.startswith("#if") or stripped.startswith("#endif"):
                problems.append(f"conditional inside BMK4_ORACLE1 guard at physical line {physical}")
            continue
        if stripped.startswith("#line "):
            try:
                value = int(stripped.split()[1])
            except (IndexError, ValueError):
                problems.append(f"unparseable #line at physical line {physical}: {stripped!r}")
                continue
            checks.append((value, len(shipping)))
            continue
        shipping.append(line)
    if in_guard:
        problems.append("unterminated BMK4_ORACLE1 guard at end of file")
    return shipping, checks, problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path("."))
    parser.add_argument("--baseline-rev", help="optional git rev to byte-compare the shipping view against")
    args = parser.parse_args()

    ok = True
    for rel in INSTRUMENTED:
        path = args.repo_root / rel
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        shipping, checks, problems = shipping_view(lines)

        for problem in problems:
            ok = False
            print(f"RED {rel}: {problem}")

        bad = [(v, idx + 1) for v, idx in checks if v != idx + 1]
        for value, expect in bad:
            ok = False
            print(f"RED {rel}: #line {value} but the next shipping-view line is {expect}")

        if args.baseline_rev:
            baseline = subprocess.run(
                ["git", "-C", str(args.repo_root), "show", f"{args.baseline_rev}:{rel}"],
                capture_output=True, text=True, check=True,
            ).stdout.splitlines()
            if shipping != baseline:
                ok = False
                print(f"RED {rel}: shipping view differs from {args.baseline_rev}")

        if not problems and not bad:
            print(f"GREEN {rel}: {len(checks)} #line restorations exact; shipping view clean")

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
