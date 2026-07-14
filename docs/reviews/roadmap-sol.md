# Adversarial review — roadmap, OAT evaluation, and Stage C/D knowledge packs (Sol)

Scope: `docs/ROADMAP_TO_PLAYABLE.md`, `docs/OAT_EVALUATION.md`,
`docs/knowledge/FF_RUNTIME_NOTES.md`, and
`docs/knowledge/RENDERER_INIT_NOTES.md`, re-evaluated against the 2026-07-14
facts that the acceptance artifact is a physical-iPad **in-game screenshot on
GitHub**, the Steam data and all five MP boot zones are present on Windows, no
local Mac is available, and audio/Bink/polish are deferred. OAT references
below use the evaluated `a87850d` revision unless a living project-status page
is explicitly named.

## CHALLENGES

1. **Claim under attack:** the Stage F main-menu milestone is on the shortest
   path to the requested artifact. **Concrete failure scenario:** the project
   posts a genuine iPad main-menu image and calls the screenshot goal met even
   though no map, client snapshot, cgame render, or world asset has run.
   **Cheapest settling test:** freeze acceptance now as a physical-iPad frame
   from `mp_killhouse`, with the producing commit and observable map/client/
   renderer postconditions; compare that contract with
   `ROADMAP_TO_PLAYABLE.md:104-123`.

2. **Claim under attack:** all remaining Stage B work should precede any
   fastfile work. **Concrete failure scenario:** B3-B5 consume several slices
   while Stage C is subsequently designed around guessed zone contents, even
   though the Windows oracle can settle the critical asset/block questions
   today. **Cheapest settling test:** implement the read-only FF0a dump mode
   first and run it locally on the five inventoried boot zones plus
   `mp_killhouse.ff`; then resume B3 → B4 → B5 unchanged. The stale assumption
   is visible in `ROADMAP_TO_PLAYABLE.md:26-54`, while the five boot zones and
   target map are confirmed at
   `ASSET_INVENTORY.md:116,118,126-127,156,174`.

3. **Claim under attack:** FF0a should emit “every loaded asset's fields” as
   one canonical artifact. **Concrete failure scenario:** real-zone names,
   strings, or payload-derived values enter CI/artifacts, violating the asset
   boundary, while a huge pointer-address-sensitive dump is neither stable nor
   useful as a structural oracle. **Cheapest settling test:** split the format:
   CI uploads only synthetic-fixture field dumps; real-zone runs remain local
   and emit a sanitized structural manifest (header/version, block sizes,
   type counts, script-string count/hash, delayed-record statistics,
   external-data references, and normalized targeted hashes). Check that a
   real-zone invocation has no upload path. The conflict is between
   `ROADMAP_TO_PLAYABLE.md:51-54,78-80` and its own no-asset rule at :17-21.

4. **Claim under attack:** OAT supports a new BMK4 layout output as if it were
   a drop-in template. **Concrete failure scenario:** C3 budgets only output
   formatting, but ZCG rejects the new template name because templates are
   registered in the C++ `CodeGenerator::SetupTemplates` map and every task is
   rendered through an asset-specific `RenderingContext`; pointer/block flags
   need new traversal semantics and tests. **Cheapest settling test:** a
   one-slice spike adds a no-op `zonelayoutmanifest` registration, emits one
   IW3 asset's complete transitive layout, and golden-tests it before BMK4
   adopts the approach. Evidence: OAT's
   [`CodeGenerator.cpp`](https://github.com/Laupetin/OpenAssetTools/blob/a87850d/src/ZoneCodeGeneratorLib/Generating/CodeGenerator.cpp)
   hard-codes `zoneload`, `zonemark`, `zonewrite`, and `assetstructtests`;
   OAT's component documentation describes ZCG as a compiled build tool, not a
   plug-in interface ([OAT components](https://openassettools.dev/guide/components.html)).

5. **Claim under attack:** matching 55 struct sizes is a sufficient hard gate
   against OAT↔Kisak schema divergence. **Concrete failure scenario:** two
   same-width fields are reordered, or an unasserted nested type differs;
   `sizeof` still agrees while generated fixups write the wrong member. The
   menu graph alone contains unasserted nested operands, expression-entry
   pointers, statements, item subtypes, and key-handler chains
   (`db_load.cpp:4462-4800`). **Cheapest settling test:** generate and compare
   `sizeof`, alignment, and every serialized member offset for the complete
   *reachable* type graph on the Windows ABI, plus tagged-union/condition
   metadata; zero mismatches is the gate. This is already mechanically
   plausible because OAT's existing
   [`AssetStructTestsTemplate.cpp`](https://github.com/Laupetin/OpenAssetTools/blob/a87850d/src/ZoneCodeGeneratorLib/Generating/Templates/AssetStructTestsTemplate.cpp)
   emits member `offsetof` checks. `OAT_EVALUATION.md:86-91` and
   `ROADMAP_TO_PLAYABLE.md:58-62` currently require sizes only.

6. **Claim under attack:** OAT's Linker is an independent correctness oracle
   for IW3 synthetic zones without a qualification gate. **Concrete failure
   scenario:** BMK4 and OAT agree on an OAT-produced fixture but both miss a
   stock-IW3 behavior, or the fixture inherits an OAT Linker defect. OAT calls
   itself work-in-progress/incomplete and currently lists an open IW3 issue,
   “Relinking a map results in broken in game rendering.” **Cheapest settling
   test:** accept each OAT fixture only after the unmodified Windows Kisak
   loader consumes it and its declared fixture values match; separately run
   OAT's x64 loader on the exact five stock boot zones and `mp_killhouse` and
   compare structural manifests. Evidence: [OAT project status](https://openassettools.dev/guide/what-is-oat.html),
   [OAT open issues, including #770](https://github.com/Laupetin/OpenAssetTools/issues),
   and `OAT_EVALUATION.md:57-60`.

7. **Claim under attack:** the “menu-only” closure is eight top-level asset
   types and SOUND can be deferred with audio playback. **Concrete failure
   scenario:** top-level type counts pass, but MENU translation corrupts its
   deep inline graph; or a non-null `focusSound` is left as a 32-bit slot and
   the loader fails before playback is relevant. `Load_itemDef_t`
   unconditionally walks `focusSound` through `Load_snd_alias_list_ptr` and
   then eight statement graphs (`db_load.cpp:4682-4737`); `Load_statement`
   expands an array of expression pointers (`db_load.cpp:4513-4555`).
   **Cheapest settling test:** the real-zone oracle reports per-zone top-level
   counts *and* non-null referenced-asset edges/union arms; a synthetic menu
   exercises `focusSound`, every statement field, at least one string operand,
   and each live item subtype. `FF_RUNTIME_NOTES.md:138-143` is therefore a
   useful top-level hypothesis, not a loader-work estimate.

8. **Claim under attack:** RAWFILE can remain an unverified menu-path detail
   while pursuing the in-game screenshot. **Concrete failure scenario:** the
   direct MP start reaches gametype discovery and cannot find
   `maps/mp/gametypes/_gametypes.txt` or the selected gametype, so the map
   never spawns even though menus render. **Cheapest settling test:** include
   RawFile names/lookups in the Windows oracle trace while launching
   `mp_killhouse`; the source already establishes the hard lookups at
   `g_scr_main_mp.cpp:6344,6364` and `ui_main_mp.cpp:4506`. The uncertainty at
   `FF_RUNTIME_NOTES.md:151` is only about a *menu*, not the requested game
   frame.

9. **Claim under attack:** C6's fixed all-assets ordering is appropriate for
   the screenshot goal. **Concrete failure scenario:** time is spent on sound
   playback trees, arbitrary menus, FX, or “remaining” asset types before the
   smallest map frame's observed closure, while required world/client types
   sit late. **Cheapest settling test:** diff sanitized type/dependency
   manifests for the five boot zones and `mp_killhouse`, then freeze only the
   closure required by a direct-map startup. One green type cluster at a time;
   no “remaining” sweep before the screenshot. The ordering under attack is
   `ROADMAP_TO_PLAYABLE.md:71-77`; OAT's official IW3 list confirms that top-
   level availability alone does not express dependencies
   ([IW3 zone entries](https://openassettools.dev/reference/zone-file.html#other-entries)).

10. **Claim under attack:** renderer work can wait until the whole Stage C
    asset sequence finishes. **Concrete failure scenario:** material loading
    is validated with `Material_UploadShaders` suppressed, then late R_Init
    exposes incompatible shader/resource fixups across many already-graduated
    waves. **Cheapest settling test:** interleave the first observed
    material/technique/image wave with engine `R_Init`; require shader creation
    and one Present before graduating the render-asset cluster. The coupling
    is already acknowledged at `FASTFILE_PLAN.md:190-194`, while the real call
    order loads boot zones inside `R_InitHardware` (`r_init.cpp:3717-3764,
    3794-3815`) and immediately initializes images, materials, and fonts
    (`RENDERER_INIT_NOTES.md:7-24`).

11. **Claim under attack:** the D1 null-descriptor work is merely an
    unverified plan assumption that should be removed for speed. **Concrete
    failure scenario:** skipping it lets Clear/Present pass but real shaders
    write illegal null descriptors for unbound slots. **Cheapest settling
    test:** retain the narrow dummy-resource patch and add an engine-like
    descriptor probe that deliberately leaves slots unbound. The claim
    survives because M12 captured the missing feature on the target device and
    audited the exact DXVK write path (`PORT_JOURNAL.md:464-468,501-512`); the
    pinned dependency is MoltenVK 1.4.1 (`ios/README.md:44-49`), irrespective
    of features added by later MoltenVK releases.

12. **Claim under attack:** a simulator console/whitescreen screenshot and a
    subsequent full-client/menu milestone are efficient acceptance proxies.
    **Concrete failure scenario:** D4 and F4 both go green without rendering a
    map on iPad, while E1 ports UI/client breadth not reached by the one target
    frame. **Cheapest settling test:** keep the simulator frame only as a
    renderer regression gate, replace the milestone with a goal-specific
    direct-`mp_killhouse` profile, and link census waves from its real closure.
    Its final marker must require a loaded map zone, renderer registered,
    active client/snapshot, cgame render, and successful Presents. Compare
    `ROADMAP_TO_PLAYABLE.md:92-114` with the requested artifact.

13. **Claim under attack:** the knowledge pack's zero-runtime-D3DX retail
    shader conclusion is too optimistic. **Concrete failure scenario:** R_Init
    reaches `D3DXCompileShader` even with retail fastfiles and the iOS link
    fails. **Cheapest settling test:** trace shader creation from the zone
    `loadDef` and enforce a link-time forbidden-symbol check for `D3DX*` in the
    fastfile configuration. The source supports the pack: streamed shader
    bytecode reaches `CreateVertexShader`/`CreatePixelShader`
    (`RENDERER_INIT_NOTES.md:36-65`), while the D3DX calls are confined to the
    loose/developer technique path. This claim should remain in the plan.

14. **Claim under attack:** the no-Mac/Sideloadly assumptions are missing from
    the roadmap. **Concrete failure scenario:** a local macOS harness becomes a
    hidden prerequisite and stops the device milestone. **Cheapest settling
    test:** search the active roadmap for install/harness requirements. The
    roadmap already says Mac-free and specifies Sideloadly
    (`ROADMAP_TO_PLAYABLE.md:1-8,104-112`); however the older FF4 text in
    `FASTFILE_PLAN.md:204-207` still names a Mac-host harness and must be
    superseded when the roadmap is revised. Hosted macOS lab probes may answer
    platform questions, but they cannot be a local-device dependency.

## VERDICTS

1. **CONFIRMED-DEFECT.** A main menu is not an in-game screenshot. Freeze the
   artifact as a real `mp_killhouse` client frame on the physical iPad; menu,
   audio, Bink, controls beyond launch, and polish are not acceptance criteria.
2. **CONFIRMED-DEFECT.** The now-available, read-only Windows oracle has the
   highest information value and should be interposed before B3. B3 → B4 → B5
   remains the correct internal dependency order and should resume immediately
   after the oracle slice; Phase 3 is not abandoned.
3. **CONFIRMED-DEFECT.** Synthetic field dumps and private real-zone structural
   manifests need separate data-handling contracts. No real-zone-derived dump
   belongs in git, CI, logs, or uploaded artifacts.
4. **CONFIRMED-DEFECT.** The OAT recommendation remains technically plausible,
   but “one new output template” is not yet demonstrated. A generator-extension
   spike is a blocking adoption gate, not implementation trivia.
5. **CONFIRMED-DEFECT.** Size equality cannot prove member-layout equality.
   The gate must cover field offsets/alignment and every serialized reachable
   type, including unasserted nested types and runtime conditions.
6. **UNTESTABLE-HERE.** OAT's x64/IW3 mechanism and limited system tests are
   strong evidence, but they do not prove the exact retail zones or every
   Linker fixture needed by BMK4. OAT stays a candidate schema/generator, never
   the correctness oracle; Windows Kisak remains authoritative.
7. **CONFIRMED-DEFECT.** The eight names are only a top-level inventory guess.
   SOUND playback may be deferred, but a non-null sound asset reference and
   the MENU transitive graph cannot be skipped by the loader.
8. **CONFIRMED-DEFECT.** RawFile is screenshot-critical unless the real launch
   trace refutes the source-reached gametype lookups. It must be in the oracle
   and translation plan even if a menu-only experiment could omit it.
9. **CONFIRMED-DEFECT.** “All types in a guessed order” optimizes for eventual
   completeness, not the requested screenshot. Oracle-derived boot + smallest-
   map reachability must define the waves.
10. **CONFIRMED-DEFECT.** Render assets and R_Init form one integration seam.
    Graduate them together early enough to expose shader/resource side effects,
    rather than carrying loud stubs across all Stage C waves.
11. **REFUTED with evidence.** D1 is not speculative: the pinned 1.4.1/device
    feature result and DXVK null-write path are recorded. What remains
    unproven is the exact dummy-resource implementation, so its gate must be a
    deliberately unbound real-shader draw, not another Clear.
12. **CONFIRMED-DEFECT.** Simulator mapless rendering remains valuable CI
    evidence, but it is neither the goal artifact nor justification for a
    full UI/client sweep. Link and run the direct-map closure first.
13. **REFUTED with evidence.** The retail shader path is precompiled and does
    not require runtime D3DX. Keep a forbidden-symbol assertion so the claim
    remains mechanically enforced.
14. **REFUTED with evidence.** The active roadmap correctly uses Windows +
    Sideloadly and defers device work. Only the older FF4 Mac-harness wording
    is stale and should be removed during plan convergence.

## PROPOSED REVISED ORDERING

These are ordered milestone slices, not permission for a big-bang commit.
Slices 8 and 9 are explicitly **wave bands**: after the oracle freezes their
membership, execute one bounded asset/TU cluster per turn, with census,
simulator, archive, and Windows gates green before the next cluster. If B2's
staging run is red, its root-cause fix precedes slice 1 and changes nothing
else.

1. **FF0a safe oracle instrument.** Add an opt-in Windows dump mode with a
   stable synthetic schema: container/version, block sizes, type counts,
   script-string metadata, delayed-record activity, external references, and
   deterministic targeted field hashes. **Gate:** Windows Debug/Release green;
   a repository-owned synthetic fixture produces a byte-stable uploaded dump;
   a test proves paths under the Steam install are refused by the artifact-
   upload job.
2. **FF0a real evidence run.** Braxton/coordinator runs that binary locally on
   the five MP boot zones and `mp_killhouse.ff`; raw dumps stay outside the
   repo. Produce only a sanitized no-content closure report (type IDs/counts,
   block/delay statistics, source-file SHA256 references) and freeze the
   screenshot asset/TU reachability hypothesis. **Gate:** all six zones load in
   the stock Windows lane; every knowledge-pack critical unknown is marked
   SETTLED or names a narrower probe.
3. **B3 — msg/net loopback.** Resume the ratified Com_Init plan with POSIX iOS
   sockets and no outbound CI traffic. **Gate:** simulator creates and passes a
   loopback packet through the real net/msg path; preflight lint, census,
   archive membership, both stub jobs, and Windows Debug/Release are green.
4. **B4 — frozen event-loop behavior.** Queue one console event through the
   real `Com_EventLoop` and observe a dvar change; do not substitute a Swift
   frame. **Gate:** the exact behavioral assertion is CI-enforced in simulator
   and the Windows lanes remain byte-identical/green.
5. **B5 — M15 closeout.** Remove replaced scaffolds and earn the exact cold-
   start marker only after Com_Init returns and every postcondition passes.
   **Gate:** `cominit=Com_Init OK — 4 subsystems up, no assets`, dvar floor and
   behavioral commands, FS write/read/delete, event probe, unsigned device IPA,
   Windows green; journal M15.
6. **OAT conformance spike and decision.** Pin `a87850d`, register a layout-
   manifest template, and emit one simple plus one deep asset graph. Compare
   every reachable Windows-ABI size/alignment/member offset and condition with
   a Kisak-derived manifest. **Gate:** zero unexplained mismatches and a golden
   generator test. If it fails, retain OAT only as a cross-check and generate
   maps from Kisak headers; do not extend the schedule by defending the tool.
7. **Synthetic FF1/FF2 kernel.** Build the minimal RawFile + StringTable zone,
   then add explicit -1, -2, alias, interior-offset, union, and delayed-block
   fixtures. Implement inflate/staging/translation and lifecycle hooks against
   the Windows oracle. **Gate:** simulator field values and structural hashes
   match the Windows dump for every fixture; overflow and provenance assertions
   stay armed; no real data in CI.
8. **Oracle-derived boot/render wave band, interleaved with R_Init.** Graduate
   only the observed boot closure, starting with material/technique/image and
   the required font/default resources; include MENU/MENULIST/SOUND only when
   the direct-map trace requires them. Land the narrow DXVK dummy-resource fix
   with the first real shader cluster. **Gate per bounded cluster:** synthetic
   round-trip vs Windows, then an engine `R_Init` simulator probe that creates
   retail-style shaders, deliberately leaves descriptor slots unbound, and
   Presents a nontrivial pixel/marker. No runtime `D3DX*` symbols.
9. **Oracle-derived `mp_killhouse` + direct-client wave band.** Graduate the
   observed RawFile/world/render/gameplay closure (expected candidates include
   XModel/XAnim, ComWorld/GameWorldMp/MapEnts, clipmap/GfxWorld, weapon/FX) and
   linker-drive only the client/cgame/server TUs reached by a direct local
   `mp_killhouse` start. UI navigation, audio, Bink, broad input, and unrelated
   asset types remain loud/deferred. **Gate per bounded cluster:** synthetic
   CI oracle match and Windows green; final band gate is an unsigned device IPA
   plus a no-proprietary-data simulator fixture proving map-zone link, local
   snapshot/cgame frame, render submission, and Present.
10. **Physical-iPad artifact and GitHub handoff.** Install the exact gated IPA
    over the existing app with Sideloadly, copy the user's complete COD4 data to
    Documents, launch directly into `mp_killhouse`, and capture the iPad screen.
    **Gate:** exported evidence names the producing commit and zone hash and
    proves map loaded, renderer registered, active client/snapshot, cgame frame,
    and successful Presents. Braxton supplies/approves the capture; only then
    the coordinator commits the screenshot (no game data) and promotes it as
    the GitHub/README hero artifact.

## STANDING

**NEEDS-FIX.** The OAT-map recommendation **survives conditionally**, and the
two knowledge packs survive as valuable source indexes, but the present arc is
not ordered for the newly frozen artifact. Before new Stage C implementation
builds on it, the coordinator should ratify: FF0a now, field-offset—not merely
size—schema gates, oracle-derived asset closure, early R_Init integration, and
a direct `mp_killhouse` physical-device acceptance contract. This review does
not block a red B2 fix, the FF0a instrument, or the already-ratified B3 → B4 →
B5 sequence after the oracle slice.
