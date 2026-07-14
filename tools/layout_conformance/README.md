# BMK4 KISAK/OAT layout conformance

This directory is the KISAK-authoritative half of the OAT adoption gate. It
compiles the repository's real headers as MSVC Win32 and emits compiler-derived
sizes, alignments, and member offsets. OAT is never used to calculate a KISAK
value.

## Why a compiler probe

The stock game ABI is Microsoft x86. A Win32 MSVC probe uses the same ABI,
preprocessor lane, and headers as the Windows regression build. A libclang
parser would add a dependency and could disagree with MSVC packing rules. The
explicit registry in `layout_registry.inc` names the serialization-reachable
field graph; `sizeof`, `alignof`, and `offsetof` supply every numeric value.

Current coverage is the complete one-structure `RawFile` graph and the 17
structures/155 members in OAT's `menuDef_t` manifest. Structure ordering is
normalized because graph order is not ABI-significant. Declaration member
order, names, sizes, alignments, and offsets are all hard comparisons.

Serialization conditions/count expressions are not derivable from C++ headers
and are intentionally outside this KISAK-side half. The adoption decision must
still compare those against the real `Load_*` paths before generated field maps
are trusted.

## Windows CI entry point

After the existing workflow has installed Python/CMake and exposed the DXSDK
include directory, each Debug/Release matrix lane runs:

```powershell
powershell -ExecutionPolicy Bypass -File tools/layout_conformance/run-windows-ci.ps1 `
  -Configuration ${{ matrix.config }} -DxSdkInclude $env:incDir
```

The script configures a separate `-A Win32` project, builds and runs the
generator twice, proves byte stability, then compares both manifests with the
checked-in OAT spike samples. The report is written to
`build-layout-conformance-<Config>/layout-conformance-report.txt`.

Any mismatch returns nonzero. There is no allowlist or alias mechanism: a
disagreement must be fixed upstream, explained and reviewed, or the OAT map
adoption fails.

## Known source-level mismatches

The initial audit already proves two real menu-schema differences:

1. KISAK declares `entryInternalData` in `op, operand` order; OAT emits
   `operand, op`.
2. KISAK names the third `operandInternalDataUnion` arm `string`; OAT names it
   `stringVal`.

Both are expected headline failures in the first hosted report. Numeric ABI
differences, if any, remain unverified until the Win32 executable runs.
