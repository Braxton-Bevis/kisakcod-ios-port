#!/usr/bin/env python3
"""Normalize KISAK/OAT layout manifests and fail on any ABI/schema mismatch."""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class Member:
    name: str
    line: int
    offset: int | None = None
    size: int | None = None
    align: int | None = None


@dataclass
class Structure:
    name: str
    line: int
    source: str | None = None
    size: int | None = None
    align: int | None = None
    expected_members: int | None = None
    members: list[Member] = field(default_factory=list)


@dataclass
class Manifest:
    path: Path
    schema: str
    asset: str
    word_size: int
    pointer_size: int
    expected_structures: int
    structures: list[Structure]


def _quoted(line: str, key: str) -> str | None:
    match = re.match(rf'^{re.escape(key)} "([^"]+)"$', line)
    return match.group(1) if match else None


def parse_manifest(path: Path) -> Manifest:
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] not in {
        "bmk4-layout-conformance-v1",
        "zone-layout-manifest-v1",
    }:
        raise ValueError(f"{path}: unsupported or missing schema")

    asset = ""
    word_size = pointer_size = expected_structures = -1
    structures: list[Structure] = []
    structure: Structure | None = None
    member: Member | None = None

    for line_number, line in enumerate(lines[1:], start=2):
        value = _quoted(line, "asset")
        if value is not None:
            asset = value
            continue
        match = re.match(r"^word-size (\d+)$", line)
        if match:
            word_size = int(match.group(1))
            continue
        match = re.match(r"^pointer-size (\d+)$", line)
        if match:
            pointer_size = int(match.group(1))
            continue
        match = re.match(r"^structure-count (\d+)$", line)
        if match:
            expected_structures = int(match.group(1))
            continue
        match = re.match(r'^structure "([^"]+)"$', line)
        if match:
            if structure is not None:
                raise ValueError(f"{path}:{line_number}: nested structure")
            structure = Structure(match.group(1), line_number)
            continue
        if line == "end-structure":
            if structure is None or member is not None:
                raise ValueError(f"{path}:{line_number}: malformed end-structure")
            if structure.expected_members != len(structure.members):
                raise ValueError(
                    f"{path}:{structure.line}: {structure.name} declares "
                    f"{structure.expected_members} members but emits "
                    f"{len(structure.members)}"
                )
            structures.append(structure)
            structure = None
            continue

        match = re.match(r'^  member \d+ "([^"]+)"$', line)
        if match:
            if structure is None or member is not None:
                raise ValueError(f"{path}:{line_number}: malformed member")
            member = Member(match.group(1), line_number)
            continue
        if line == "  end-member":
            if structure is None or member is None:
                raise ValueError(f"{path}:{line_number}: malformed end-member")
            if None in (member.offset, member.size, member.align):
                raise ValueError(
                    f"{path}:{member.line}: incomplete layout for {member.name}"
                )
            structure.members.append(member)
            member = None
            continue

        if member is not None:
            match = re.match(r"^    (offset|size|align) (\d+)$", line)
            if match:
                setattr(member, match.group(1), int(match.group(2)))
            continue
        if structure is not None:
            value = _quoted(line, "  source")
            if value is not None:
                structure.source = value
                continue
            match = re.match(r"^  (size|align|member-count) (\d+)$", line)
            if match:
                attribute = {
                    "size": "size",
                    "align": "align",
                    "member-count": "expected_members",
                }[match.group(1)]
                setattr(structure, attribute, int(match.group(2)))

    if structure is not None or member is not None:
        raise ValueError(f"{path}: unterminated structure/member")
    if not asset or word_size < 0 or pointer_size < 0 or expected_structures < 0:
        raise ValueError(f"{path}: incomplete manifest header")
    if expected_structures != len(structures):
        raise ValueError(
            f"{path}: declares {expected_structures} structures but emits "
            f"{len(structures)}"
        )
    for item in structures:
        if None in (item.size, item.align, item.expected_members):
            raise ValueError(f"{path}:{item.line}: incomplete {item.name} layout")
    return Manifest(
        path, lines[0], asset, word_size, pointer_size, expected_structures, structures
    )


def _structure_span(lines: list[str], name: str) -> tuple[int, int] | None:
    declaration = re.compile(rf"^\s*(?:struct|union)\s+{re.escape(name)}\b")
    for index, line in enumerate(lines):
        if not declaration.search(line):
            continue
        depth = 0
        opened = False
        for end in range(index, len(lines)):
            depth += lines[end].count("{")
            if "{" in lines[end]:
                opened = True
            depth -= lines[end].count("}")
            if opened and depth == 0:
                return index, end
        return index, index
    return None


def kisak_context(
    repo_root: Path, structure: Structure, member_name: str | None = None
) -> str:
    if not structure.source:
        return "KISAK-source-unavailable"
    source = repo_root / structure.source
    try:
        lines = source.read_text(encoding="utf-8").splitlines()
    except OSError:
        return f"{structure.source}:?"
    span = _structure_span(lines, structure.name)
    if span is None:
        return f"{structure.source}:?"
    start, end = span
    if member_name:
        member = re.compile(
            rf"\b{re.escape(member_name)}\b\s*(?:\[[^\]]*\]\s*)*;"
        )
        for index in range(start, end + 1):
            stripped = lines[index].strip()
            if "=" in stripped or stripped.startswith("return "):
                continue
            if member.search(lines[index]):
                return f"{structure.source}:{index + 1}"
    return f"{structure.source}:{start + 1}"


def oat_context(manifest: Manifest, record: Structure | Member) -> str:
    return f"{manifest.path.as_posix()}:{record.line}"


def _names(items: list[Structure] | list[Member]) -> list[str]:
    return [item.name for item in items]


def compare_pair(kisak: Manifest, oat: Manifest, repo_root: Path) -> list[str]:
    failures: list[str] = []

    def fail(kind: str, message: str, kctx: str, octx: str) -> None:
        failures.append(
            f'MISMATCH {kind}: asset="{kisak.asset}" {message} '
            f"kisak={kctx} oat={octx}"
        )

    header_context = f"{kisak.path.as_posix()}:1"
    oat_header_context = f"{oat.path.as_posix()}:1"
    if kisak.asset != oat.asset:
        fail(
            "asset",
            f'kisak-name="{kisak.asset}" oat-name="{oat.asset}"',
            header_context,
            oat_header_context,
        )
        return failures
    for label, kvalue, ovalue in (
        ("word-size", kisak.word_size, oat.word_size),
        ("pointer-size", kisak.pointer_size, oat.pointer_size),
    ):
        if kvalue != ovalue:
            fail(label, f"kisak-value={kvalue} oat-value={ovalue}", header_context, oat_header_context)

    kstructs = {item.name: item for item in kisak.structures}
    ostructs = {item.name: item for item in oat.structures}
    for name in sorted(kstructs.keys() - ostructs.keys()):
        item = kstructs[name]
        fail(
            "structure-only-kisak",
            f'structure="{name}"',
            kisak_context(repo_root, item),
            oat_header_context,
        )
    for name in sorted(ostructs.keys() - kstructs.keys()):
        item = ostructs[name]
        fail(
            "structure-only-oat",
            f'structure="{name}"',
            header_context,
            oat_context(oat, item),
        )

    for name in sorted(kstructs.keys() & ostructs.keys()):
        kstruct = kstructs[name]
        ostruct = ostructs[name]
        ksource = kisak_context(repo_root, kstruct)
        osource = oat_context(oat, ostruct)
        for label, kvalue, ovalue in (
            ("structure-size", kstruct.size, ostruct.size),
            ("structure-align", kstruct.align, ostruct.align),
        ):
            if kvalue != ovalue:
                fail(
                    label,
                    f'structure="{name}" kisak-value={kvalue} oat-value={ovalue}',
                    ksource,
                    osource,
                )

        korder = _names(kstruct.members)
        oorder = _names(ostruct.members)
        if korder != oorder:
            fail(
                "member-order",
                f'structure="{name}" kisak-order={korder!r} oat-order={oorder!r}',
                ksource,
                osource,
            )

        kmembers = {item.name: item for item in kstruct.members}
        omembers = {item.name: item for item in ostruct.members}
        for member_name in sorted(kmembers.keys() - omembers.keys()):
            member = kmembers[member_name]
            fail(
                "member-only-kisak",
                f'structure="{name}" member="{member_name}"',
                kisak_context(repo_root, kstruct, member_name),
                osource,
            )
        for member_name in sorted(omembers.keys() - kmembers.keys()):
            member = omembers[member_name]
            fail(
                "member-only-oat",
                f'structure="{name}" member="{member_name}"',
                ksource,
                oat_context(oat, member),
            )
        for member_name in sorted(kmembers.keys() & omembers.keys()):
            kmember = kmembers[member_name]
            omember = omembers[member_name]
            for label in ("offset", "size", "align"):
                kvalue = getattr(kmember, label)
                ovalue = getattr(omember, label)
                if kvalue != ovalue:
                    fail(
                        f"member-{label}",
                        f'structure="{name}" member="{member_name}" '
                        f"kisak-value={kvalue} oat-value={ovalue}",
                        kisak_context(repo_root, kstruct, member_name),
                        oat_context(oat, omember),
                    )
    return failures


def run_self_test() -> int:
    base = """bmk4-layout-conformance-v1
source kisak-msvc-x86
game "IW3"
word-size 32
pointer-size 4
asset "RawFile"
structure-count 1
structure "RawFile"
  kind struct
  source "missing.h"
  size 12
  align 4
  member-count 1
  member 0 "name"
    offset 0
    size 4
    align 4
  end-member
end-structure
"""
    oat = base.replace("bmk4-layout-conformance-v1\nsource kisak-msvc-x86\n", "zone-layout-manifest-v1\n")
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        kisak_path = root / "kisak.layout"
        oat_path = root / "oat.layout"
        kisak_path.write_text(base, encoding="utf-8")
        oat_path.write_text(oat, encoding="utf-8")
        if compare_pair(parse_manifest(kisak_path), parse_manifest(oat_path), root):
            raise AssertionError("equal self-test manifests did not compare equal")
        oat_path.write_text(oat.replace("    offset 0", "    offset 4"), encoding="utf-8")
        failures = compare_pair(parse_manifest(kisak_path), parse_manifest(oat_path), root)
        if len(failures) != 1 or "member-offset" not in failures[0]:
            raise AssertionError("offset mismatch self-test was not precise")
    print("layout comparator self-test PASS")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path)
    parser.add_argument("--pair", action="append", default=[], metavar="KISAK=OAT")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return run_self_test()
    if not args.repo_root or not args.report or not args.pair:
        parser.error("--repo-root, --report, and at least one --pair are required")

    repo_root = args.repo_root.resolve()
    all_failures: list[str] = []
    pair_lines: list[str] = []
    try:
        for pair in args.pair:
            if "=" not in pair:
                raise ValueError(f"invalid --pair {pair!r}; expected KISAK=OAT")
            kisak_name, oat_name = pair.split("=", 1)
            kisak = parse_manifest(Path(kisak_name))
            oat = parse_manifest(Path(oat_name))
            failures = compare_pair(kisak, oat, repo_root)
            pair_lines.append(
                f'pair asset="{kisak.asset}" status='
                f'{"PASS" if not failures else "MISMATCH"} mismatches={len(failures)}'
            )
            all_failures.extend(failures)
    except (OSError, ValueError) as error:
        print(f"layout comparison failed: {error}", file=sys.stderr)
        return 2

    report_lines = ["layout-conformance-report-v1", *pair_lines, *all_failures]
    report_lines.append(
        f"summary pairs={len(args.pair)} mismatches={len(all_failures)} "
        f'standing={"SURVIVES" if not all_failures else "NEEDS-FIX"}'
    )
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text("\n".join(report_lines) + "\n", encoding="utf-8")
    print("\n".join(report_lines))
    return 1 if all_failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
