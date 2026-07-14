#!/usr/bin/env python3
"""Normalize KISAK/OAT layout manifests and fail on any ABI/schema mismatch."""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path


# Coordinator adjudication after Windows run 29358731938: OAT's stringVal and
# KISAK's string name the same operandInternalDataUnion arm. Keep this table
# structure-specific so no unrelated schema difference can inherit the ruling.
ADJUDICATIONS: dict[str, dict[str, str]] = {
    "operandInternalDataUnion": {
        # OAT member name -> KISAK member name (run 29358731938 final ruling).
        "stringVal": "string",
    },
}


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
    kind: str | None = None
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


@dataclass
class ComparisonResult:
    failures: list[str] = field(default_factory=list)
    adjudications: list[str] = field(default_factory=list)
    aliases: int = 0
    union_order_exemptions: int = 0


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
            match = re.match(r"^  kind (struct|union)$", line)
            if match:
                structure.kind = match.group(1)
                continue
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
        if None in (item.kind, item.size, item.align, item.expected_members):
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


def compare_pair(kisak: Manifest, oat: Manifest, repo_root: Path) -> ComparisonResult:
    result = ComparisonResult()

    def fail(kind: str, message: str, kctx: str, octx: str) -> None:
        result.failures.append(
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
        return result
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
        if kstruct.kind != ostruct.kind:
            fail(
                "structure-kind",
                f'structure="{name}" kisak-kind="{kstruct.kind}" '
                f'oat-kind="{ostruct.kind}"',
                ksource,
                osource,
            )
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

        is_union = kstruct.kind == ostruct.kind == "union"
        configured_aliases = ADJUDICATIONS.get(name, {})
        if configured_aliases and not is_union:
            fail(
                "adjudication-requires-union",
                f'structure="{name}" alias-count={len(configured_aliases)}',
                ksource,
                osource,
            )
        aliases = configured_aliases if is_union else {}
        kmembers = {item.name: item for item in kstruct.members}
        omembers: dict[str, tuple[Member, str]] = {}
        oorder: list[str] = []
        for omember in ostruct.members:
            oat_name = omember.name
            kisak_name = aliases.get(oat_name, oat_name)
            if kisak_name in omembers:
                previous = omembers[kisak_name][1]
                fail(
                    "member-alias-collision",
                    f'structure="{name}" oat-members={[previous, oat_name]!r} '
                    f'kisak-member="{kisak_name}"',
                    ksource,
                    oat_context(oat, omember),
                )
                continue
            omembers[kisak_name] = (omember, oat_name)
            oorder.append(kisak_name)
            if oat_name != kisak_name:
                result.aliases += 1
                result.adjudications.append(
                    f'ADJUDICATED member-alias: asset="{kisak.asset}" '
                    f'structure="{name}" oat-member="{oat_name}" '
                    f'kisak-member="{kisak_name}" evidence="Windows run 29358731938"'
                )

        korder = _names(kstruct.members)
        if is_union and korder != oorder:
            result.union_order_exemptions += 1
            result.adjudications.append(
                f'ADJUDICATED union-member-order: asset="{kisak.asset}" '
                f'structure="{name}" kisak-order={korder!r} oat-order={oorder!r} '
                f'evidence="Windows run 29358731938"'
            )
        elif not is_union and korder != oorder:
            fail(
                "member-order",
                f'structure="{name}" kisak-order={korder!r} oat-order={oorder!r}',
                ksource,
                osource,
            )

        for member_name in sorted(kmembers.keys() - omembers.keys()):
            member = kmembers[member_name]
            fail(
                "member-only-kisak",
                f'structure="{name}" member="{member_name}"',
                kisak_context(repo_root, kstruct, member_name),
                osource,
            )
        for member_name in sorted(omembers.keys() - kmembers.keys()):
            member, oat_name = omembers[member_name]
            fail(
                "member-only-oat",
                f'structure="{name}" oat-member="{oat_name}" '
                f'normalized-member="{member_name}"',
                ksource,
                oat_context(oat, member),
            )
        for member_name in sorted(kmembers.keys() & omembers.keys()):
            kmember = kmembers[member_name]
            omember, oat_name = omembers[member_name]
            for label in ("offset", "size", "align"):
                kvalue = getattr(kmember, label)
                ovalue = getattr(omember, label)
                if kvalue != ovalue:
                    member_description = (
                        f'kisak-member="{member_name}" oat-member="{oat_name}"'
                        if oat_name != member_name
                        else f'member="{member_name}"'
                    )
                    fail(
                        f"member-{label}",
                        f'structure="{name}" {member_description} '
                        f"kisak-value={kvalue} oat-value={ovalue}",
                        kisak_context(repo_root, kstruct, member_name),
                        oat_context(oat, omember),
                    )
    return result


def pair_status_line(asset: str, result: ComparisonResult) -> str:
    if result.failures:
        status = "MISMATCH"
    elif result.aliases or result.union_order_exemptions:
        status = "PASS-ADJUDICATED"
    else:
        status = "PASS"
    line = f'pair asset="{asset}" status={status}'
    if result.aliases or result.union_order_exemptions:
        line += (
            f" aliases={result.aliases}"
            f" union-order-exemptions={result.union_order_exemptions}"
        )
    return f"{line} mismatches={len(result.failures)}"


def run_self_test() -> int:
    def render(
        schema: str,
        asset: str,
        structure: str,
        kind: str,
        structure_size: int,
        members: list[tuple[str, int, int]],
    ) -> str:
        lines = [schema]
        if schema == "bmk4-layout-conformance-v1":
            lines.append("source kisak-msvc-x86")
        lines.extend(
            [
                'game "IW3"',
                "word-size 32",
                "pointer-size 4",
                f'asset "{asset}"',
                "structure-count 1",
                f'structure "{structure}"',
                f"  kind {kind}",
                '  source "missing.h"',
                f"  size {structure_size}",
                "  align 4",
                f"  member-count {len(members)}",
            ]
        )
        for index, (name, offset, size) in enumerate(members):
            lines.extend(
                [
                    f'  member {index} "{name}"',
                    f"    offset {offset}",
                    f"    size {size}",
                    "    align 4",
                    "  end-member",
                ]
            )
        lines.append("end-structure")
        return "\n".join(lines) + "\n"

    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        kisak_path = root / "kisak.layout"
        oat_path = root / "oat.layout"

        base = render(
            "bmk4-layout-conformance-v1",
            "RawFile",
            "RawFile",
            "struct",
            12,
            [("name", 0, 4)],
        )
        oat = render(
            "zone-layout-manifest-v1",
            "RawFile",
            "RawFile",
            "struct",
            12,
            [("name", 0, 4)],
        )
        kisak_path.write_text(base, encoding="utf-8")
        oat_path.write_text(oat, encoding="utf-8")
        result = compare_pair(parse_manifest(kisak_path), parse_manifest(oat_path), root)
        if result.failures:
            raise AssertionError("equal self-test manifests did not compare equal")
        oat_path.write_text(
            oat.replace("    offset 0", "    offset 4"), encoding="utf-8"
        )
        result = compare_pair(parse_manifest(kisak_path), parse_manifest(oat_path), root)
        if len(result.failures) != 1 or "member-offset" not in result.failures[0]:
            raise AssertionError("offset mismatch self-test was not precise")

        kisak_path.write_text(
            render(
                "bmk4-layout-conformance-v1",
                "OrderTest",
                "OrderTest",
                "struct",
                8,
                [("left", 0, 4), ("right", 4, 4)],
            ),
            encoding="utf-8",
        )
        oat_path.write_text(
            render(
                "zone-layout-manifest-v1",
                "OrderTest",
                "OrderTest",
                "struct",
                8,
                [("right", 4, 4), ("left", 0, 4)],
            ),
            encoding="utf-8",
        )
        result = compare_pair(parse_manifest(kisak_path), parse_manifest(oat_path), root)
        if len(result.failures) != 1 or "member-order" not in result.failures[0]:
            raise AssertionError("struct member order self-test was not strict")

        kisak_path.write_text(
            render(
                "bmk4-layout-conformance-v1",
                "UnionOrderTest",
                "UnionOrderTest",
                "union",
                4,
                [("left", 0, 4), ("right", 0, 4)],
            ),
            encoding="utf-8",
        )
        oat_path.write_text(
            render(
                "zone-layout-manifest-v1",
                "UnionOrderTest",
                "UnionOrderTest",
                "union",
                4,
                [("right", 0, 4), ("left", 0, 4)],
            ),
            encoding="utf-8",
        )
        result = compare_pair(parse_manifest(kisak_path), parse_manifest(oat_path), root)
        if result.failures or result.union_order_exemptions != 1:
            raise AssertionError("union member-order exemption self-test failed")

        kisak_alias = render(
            "bmk4-layout-conformance-v1",
            "menuDef_t",
            "operandInternalDataUnion",
            "union",
            8,
            [("intVal", 0, 4), ("floatVal", 0, 4), ("string", 0, 4)],
        )
        oat_alias = render(
            "zone-layout-manifest-v1",
            "menuDef_t",
            "operandInternalDataUnion",
            "union",
            8,
            [("intVal", 0, 4), ("floatVal", 0, 4), ("stringVal", 0, 4)],
        )
        kisak_path.write_text(kisak_alias, encoding="utf-8")
        oat_path.write_text(oat_alias, encoding="utf-8")
        result = compare_pair(parse_manifest(kisak_path), parse_manifest(oat_path), root)
        if result.failures or result.aliases != 1:
            raise AssertionError("adjudicated member-alias self-test failed")
        expected_status = (
            'pair asset="menuDef_t" status=PASS-ADJUDICATED '
            "aliases=1 union-order-exemptions=0 mismatches=0"
        )
        if pair_status_line("menuDef_t", result) != expected_status:
            raise AssertionError("adjudicated status self-test failed")
        if not any("stringVal" in line and 'kisak-member="string"' in line for line in result.adjudications):
            raise AssertionError("adjudicated member-alias report self-test failed")

        oat_path.write_text(
            oat_alias.replace(
                '  member 2 "stringVal"\n    offset 0\n    size 4',
                '  member 2 "stringVal"\n    offset 0\n    size 8',
            ),
            encoding="utf-8",
        )
        result = compare_pair(parse_manifest(kisak_path), parse_manifest(oat_path), root)
        if not any("member-size" in failure for failure in result.failures):
            raise AssertionError("aliased numeric divergence did not fail")
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
    all_adjudications: list[str] = []
    pair_lines: list[str] = []
    total_aliases = 0
    total_union_order_exemptions = 0
    try:
        for pair in args.pair:
            if "=" not in pair:
                raise ValueError(f"invalid --pair {pair!r}; expected KISAK=OAT")
            kisak_name, oat_name = pair.split("=", 1)
            kisak = parse_manifest(Path(kisak_name))
            oat = parse_manifest(Path(oat_name))
            result = compare_pair(kisak, oat, repo_root)
            pair_lines.append(pair_status_line(kisak.asset, result))
            all_adjudications.extend(result.adjudications)
            all_failures.extend(result.failures)
            total_aliases += result.aliases
            total_union_order_exemptions += result.union_order_exemptions
    except (OSError, ValueError) as error:
        print(f"layout comparison failed: {error}", file=sys.stderr)
        return 2

    report_lines = [
        "layout-conformance-report-v1",
        *pair_lines,
        *all_adjudications,
        *all_failures,
    ]
    report_lines.append(
        f"summary pairs={len(args.pair)} mismatches={len(all_failures)} "
        f"aliases={total_aliases} "
        f"union-order-exemptions={total_union_order_exemptions} "
        f'standing={"SURVIVES" if not all_failures else "NEEDS-FIX"}'
    )
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text("\n".join(report_lines) + "\n", encoding="utf-8")
    print("\n".join(report_lines))
    return 1 if all_failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
