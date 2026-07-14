# BMK4 slice 6 spike: IW3 zone layout manifests

## Verdict

**FEASIBLE.** `ZoneCodeGenerator` can expose its computed IW3 layout and serialization metadata through a new hard-registered output template. The spike emits deterministic manifests from `OncePerAssetRenderingContext` at the game word size (IW3 is 32-bit), without parsing the C header a second time.

The context's closure is the same one used by the load/mark/write templates: inline serialization-reachable structures are traversed transitively; foreign asset references and runtime-block members are terminal edges. The manifest states these boundaries explicitly. Every member of every reached structure is still emitted, including statically ignored union arms, so size/alignment/offset comparisons do not hide ABI fields.

## Evidence

- `RawFile`: one structure, three members, 32-bit size 12/alignment 4; `buffer` has the generated count expression `len + 1`.
- `menuDef_t` (`AssetMenu`): 17 structures and 155 members. Its closure includes `ItemKeyHandler`, `statement_s`, `expressionEntry`, `Operand`, `itemDef_s`, and `itemDefData_t`, as well as terminal `Material` and `snd_alias_list_t` asset edges.
- The deep manifest records the `expressionEntry` operand condition, all four `itemDefData_t` live-arm conditions plus the statically disabled arm, the menu item/statement counts, block assignments, pointer/array shapes, and per-member offsets/sizes/alignments.
- Two independent generations produced byte-identical output for all 25 IW3 assets. The checked-in RawFile and Menu goldens matched those runs byte for byte.

Golden files:

- `test/ZoneCodeGeneratorLibTests/Golden/IW3/ZoneLayoutManifest/iw3_rawfile.layout`
- `test/ZoneCodeGeneratorLibTests/Golden/IW3/ZoneLayoutManifest/iw3_menudef_t.layout`

## Exact build and run commands

The spike host did not have Visual Studio installed, so the minimal proof used portable Zig 0.13.0 as the C++23 driver. From the OAT repository root:

```powershell
New-Item -ItemType Directory -Force build | Out-Null
node -e "const https=require('https'),fs=require('fs');https.get('https://ziglang.org/download/0.13.0/zig-windows-x86_64-0.13.0.zip',r=>r.pipe(fs.createWriteStream('build/zig-0.13.0.zip')))"
& 'C:\Program Files\Git\usr\bin\unzip.exe' -o build\zig-0.13.0.zip -d build
powershell -ExecutionPolicy Bypass -File tools\spikes\bmk4-slice6\Build-ZoneCodeGenerator.ps1 -ZigPath build\zig-windows-x86_64-0.13.0\zig.exe
powershell -ExecutionPolicy Bypass -File tools\spikes\bmk4-slice6\Verify-ZoneLayoutManifest.ps1
```

The verifier runs the equivalent generator command twice, compares every generated IW3 manifest byte for byte, then compares the two requested outputs with the checked-in goldens:

```powershell
build\bmk4-spike\ZoneCodeGenerator.exe --no-color -h src\ZoneCode\Game\IW3\IW3_ZoneCode.h -c src\ZoneCode\Game\IW3\IW3_Commands.txt -o build\bmk4-spike\golden-test\run1 -g ZoneLayoutManifest
```

## Full adoption estimate and BMK4 consumption

Estimate: **3-5 engineer-days** after this spike. Allow 1-2 days to freeze/version the manifest schema and add focused C++ tests for typedefs, anonymous aggregates, indexed counts, bitfields, runtime blocks, and foreign assets; 1-2 days for a BMK4 parser/normalizer and comparison report; and about one day to run the full IW3 set and triage genuine OAT/Kisak divergences.

BMK4 should consume only normalized records needed by its translation gate: game/pointer size, structure kind/name/size/alignment, member name/type/offset/size/alignment, declaration modifier shape, count/condition/allocation expressions, block identity/type, and string/script-string/reusable/asset-reference flags. Kisak's own headers and Windows behavior remain authoritative; OAT supplies a mechanically independent schema and generator cross-check.
