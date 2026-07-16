# Lane B — renderer device-enablement: plan + adversarial review log

Lane B (Claude Fable 5) implements; ChatGPT 5.6 Sol (ultra) is the adversarial
critic. Branch `renderer-device-enable` from `main` @ `6475ec7`. Mission:
device-enable the preserved placeholder-renderer wave
(`renderer-placeholder-queued` @ `5121928`) so the owner's first real-GPU
engine-path screenshot is possible via sideloading, per Amendment 1 in
`docs/reviews/pre-mac-plan-claude-response.md` (device-enablement REPLACES the
hosted simulator sampler fallback).

## Round 1 — rebase plan (kept vs dropped, commit/hunk level)

### Three-way picture

- `main` = `6475ec7` (census 42, ff-kernel merged; sim job links NO DXVK;
  device job links the full DXVK/MoltenVK stack).
- `staging` = `91b81c1`, whose renderer part (`f780d19`..`90d835d`) is the
  simulator-DXVK plumbing wave — red at the
  `shaderSampledImageArrayDynamicIndexing` sampler boundary, NOT on main.
- `renderer-placeholder-queued` = staging@`90d835d` + `c2bd68c` (bound sim
  link closure) + `5121928` (generated IW3 R/RB scene, census 51,
  libkisakrenderer, SIMULATOR-bounded, never ran in CI).

### Dropped wholesale

- **All seven staging commits `f780d19`..`90d835d`** (sim MoltenVK-all slice,
  sim DXVK link, imageCubeArray/viewport/occlusion/shaderInt64 admissions):
  simulator-DXVK plumbing; main's sim job must stay exactly as green as on
  main, and Amendment 1 defers the hosted sampler fallback entirely.
- **`c2bd68c`'s sim-side hunks** (its mechanism is ADOPTED on device per Sol
  round 1 — see the project.yml row and finding 1):
  (1) ios-stub.yml sim `_vkGetInstanceProcAddr` nm gate (sim links no
  MoltenVK on main → would fail green sim job); the equivalent gate now
  lives in the DEVICE job against the packaged IPA binary;
  (2) project.yml `DEAD_CODE_STRIPPING[sdk=iphonesimulator*]: YES` +
  sim `-Wl,-u,_vkGetInstanceProcAddr` (sim links no DXVK/MoltenVK on main;
  the dead-strip+root pair moves to `[sdk=iphoneos*]` where it bounds the
  renderer closure);
  (3) build-dxvk-ios.sh comment rewording describing the sim experiment.

### From `5121928` — kept / adapted / dropped, per file

| File | Disposition |
|---|---|
| `src/gfx_d3d/r_rendercmds.cpp/.h`, `rb_backend.cpp`, `r_init.cpp` | **KEPT VERBATIM plus one swept LP64 fix** (git checkout from `5121928`; rb_backend.cpp additionally gains the fenced `RB_AdaptiveGpuSyncFinal` rewrite from the sweep below). Zero drift between the queued base and main for `src/gfx_d3d/**`. All edits are `#ifdef KISAK_IOS` with byte-identical `#else`: the RC_DRAW_TRIANGLES producer seam (`R_iOS_ResetCommandList` / `R_iOS_QueueDrawTrianglesCommand` / `R_iOS_EndCommandList` private aligned command list + restore), the `rg.inFrame \|\| s_iOSCommandFrameActive` iassert relaxation in `R_GetCommandBuffer`, fail-closed abort-loud fences on the deferred lifecycle owners (`R_InitThreads`, `R_SyncRenderThread`, `R_ComErrorCleanup`, remote-screen-update trio, debug-frame pair), and the two LP64 consumer fixes (24-byte arm64 `GfxCmdDrawTriangles` offset; pointer-to-array UB fix in `RB_DrawTriangles_Internal`). |
| `ios/Stub/RendererPlaceholder.cpp/.h` | **KEPT, gate INVERTED** `#if TARGET_OS_SIMULATOR` → `#if !TARGET_OS_SIMULATOR`. The scene, typed generated material, committed SM3 token streams, exact-advance/stats/tess-drain/uploads/readback earn logic are unchanged. Comments updated: device proof; simulator refuses honestly. |
| `ios/Stub/RendererProofScaffold.cpp` | **KEPT, gate INVERTED** to device. Provides `rg`, `pixelCostMode`, the 15 `r_*`/`sm_*` dvar pointer definitions, abort-loud deferred owners (RB_Log\*, R_PixelCost\*, R_UploadWaterTexture) for the device link only. |
| `ios/Stub/BootScaffold.cpp` | **ADAPTED, gating INVERTED**: the nine graduated scaffolds (`R_InitThreads`, `R_ComErrorCleanup`, `R_SyncRenderThread`, `R_Begin/End/PopRemoteScreenUpdate`, `R_Begin/EndDebugFrame`, `Sys_DestroySplashWindow`) are now wrapped `#if TARGET_OS_SIMULATOR` (kept for sim, which does not link libkisakrenderer; deleted on device where the real owners enter the link). `R_CmdBufSet3D` abort-loud scaffold + `GfxCmdBufSourceState` fwd decl + `<TargetConditionals.h>` added unconditionally (r_cmdbuf.cpp stays outside the archive). `R_PopRemoteScreenUpdate` scaffold signature corrected to `int`. |
| `ios/Stub/D3D9Smoke.mm` | **RE-AUTHORED against main's version** (queued's diff was against staging's rewritten smoke, which doesn't exist on main). Device section: capture `clearHr` (previously discarded) and retain `d3d`/`dev` in statics only when the smoke actually earned (`SUCCEEDED(clearHr) && SUCCEEDED(readHr) && pixel==0xFFBA55D3 && SUCCEEDED(presentHr)` — amended per Sol round 1 finding 2); the returned status string is byte-unchanged. New `kisak_renderer_placeholder()` / `_detail()` bridges: device = guarded (try/catch) call into `kisak_renderer_placeholder_run(d3d, dev)`, refusing when the smoke didn't earn; simulator = honest `device-only stage, not run (no DXVK simulator build)`. No exact-marker "OK" string is introduced for the d3d9 smoke (main's device smoke reports the raw adapter/clear/read/px/present line; changing it is out of scope). |
| `ios/Stub/MetalViewController.swift` | **ADAPTED**: `rendererStatus`/`rendererDetailStatus` state; the scene stage runs unconditionally after the d3d9 smoke (the native bridge is the capability gate — no Swift `#if targetEnvironment`, unlike queued which keyed on staging's exact sim d3d9-OK string); crash sentinel now stays armed through the scene stage; HUD `renderer:` line; marker `render=`/`render-detail=` lines; on scene success (device-only reachable): 4:3 canvas fit of the d3d9 layer + honest "generated placeholder scene; no retail assets; not mp_killhouse" label + virtual-controller disconnect for a clean evidence shot. Dropped: queued's sim-iPad UDID jq change and everything tied to staging's sim runtime. |
| `ios/Stub/BridgingHeader.h` | **KEPT** (two new decls), comment flipped to device-runs / simulator-refuses. |
| `ios/project.yml` | **ADAPTED (amended per Sol round 1)**: device-only hunks — prepend `-lkisakrenderer` to `OTHER_LDFLAGS[sdk=iphoneos*]`, add `DEAD_CODE_STRIPPING[sdk=iphoneos*]: YES` + `-Wl,-u,_vkGetInstanceProcAddr` (the link-bounding mechanism c2bd68c pioneered, now applied where it is load-bearing: the device). Base (=sim) OTHER_LDFLAGS, base DEAD_CODE_STRIPPING: NO, and every other setting byte-identical to main. |
| `scripts/ios/CMakeLists.txt` | **KEPT**: same 10 TUs appended (r_rendercmds, rb_backend, rb_shade, rb_state, rb_stats, r_state_utils, r_shade, r_state, r_buffers, r_draw_bsp). Census 42 → 52 (queued said 51 because its base predated ff_kernel #42). |
| `.github/workflows/ios-compile-probe.yml` | **ADAPTED**: floor `-ge 42` → `-ge 52` (queued: 41→51). |
| `scripts/platform/ios/build-engine-lib.sh` | **KEPT + EXTENDED**: hygiene hunks (rm -rf OBJDIR; stale-archive rm — extended with `libkisakff.a`, which queued's base didn't have; `\|\| return 1` hardening on the archive/lipo lines) and the exact 11-member `libkisakrenderer.a` block with membership diff, placed after the ffk block. Built for BOTH SDKs — the sim lane therefore still COMPILES + ARCHIVES every renderer TU (sim compile coverage) while linking none of it. |
| `.github/workflows/ios-stub.yml` | **SPLIT**: sim-job hunks **DROPPED entirely** (renderer ar/nm checks on the sim archive, demangled sim-app-binary greps, `render=` exact marker gate, `render-detail=` regex gate, iPad-first jq, timeout message — all were sim gates; task forbids new sim gates and sim behavior changes). Device-job hunks **NEWLY AUTHORED** mirroring the sim job's ar/nm grep style: (1) in `build_renderer`, exact `libkisakrenderer.a` membership (11 members) + producer/consumer + lifecycle-owner symbol greps on the archive; (2) in `package_ipa`, `nm -gU` of the app binary inside `Payload/` grepping the scene entry (`_kisak_renderer_placeholder`, demangled `kisak_renderer_placeholder_run(`, `R_GetCommandBuffer(`, `R_iOS_QueueDrawTrianglesCommand(`, `RB_DrawTrianglesCmd(`, `RB_EndTessSurface(`, `R_DrawTessTechnique(`, `R_DrawIndexedPrimitive(`); evidence tee'd to files and added to the artifact upload. NO runtime/marker gate for the scene — that gate is added only when device-runtime evidence exists (owner's sideload sitting). |
| `docs/NEXT_SESSION.md` | 'Lane B device-enablement' subsection appended: intended exact device marker lines, gate plan, sideload instructions. |

### Sim-job preservation claim

The sim job's workflow text is untouched except nothing — zero hunks. The only
sim-lane deltas are: (a) build-engine-lib.sh now also archives
libkisakrenderer for the sim SDK (a *build-level strengthening*: a renderer TU
that fails to compile for sim now fails the sim job's build step — compile
coverage, not a new marker gate); (b) the app compiles the new stub files,
whose sim halves are inert/honest-refusal; (c) the marker file gains
`render=device-only stage, not run (no DXVK simulator build)` and a matching
`render-detail=` line — additive lines; every existing `grep -Fqx` gate is
unaffected. BootScaffold's sim half is byte-equivalent to main's scaffold set
(plus the never-reached `R_CmdBufSet3D` addition and the `int` return-type
correction, which does not change the mangled symbol).

### Intended device marker lines (documented now, CI-gated only after DEVICE_RUN evidence)

```
render=IW3 R/RB placeholder scene OK — generated assets, RC_DRAW_TRIANGLES, readback non-background, Present
render-detail=IW3 R/RB placeholder detail — vertices=339 indices=483 triangles=161 cmdBytes=14552 stats=1/161/339/483 changedPixels=<decimal ≥153600> fnv1a=0x<8 hex> center=0x<8 hex> uploads=10848/483
```

Earned only after: exact command-cursor advance, prim stats 1/161/339/483,
tess drained, dynamic buffer uploads 10848/483 bytes/indices, readback
non-background on ≥50% of 640×480 pixels, and successful Present — all
computed natively in C++ (Swift only transcribes).

### LP64 sweep of the enabled TUs (device is acceptance hardware)

Classes swept across all 10 new TUs (+ retained r_init.cpp):
- `qsort` with literal element sizes: **none present**.
- `uint32`/`int` casts of pointers, pointer truncation to int/int16 handles:
  **none found** (the two grep hits are enum/count range checks).
- `LOWORD(tess.vertexCount)` index emission (many sites, rb_backend.cpp):
  benign by design — counts are bounded far below 64K and the index stream is
  D3DFMT_INDEX16; not pointer truncation.
- **Found and fixed**: `RB_AdaptiveGpuSyncFinal` (rb_backend.cpp:2647) is a
  decompiler register artifact that writes only `LODWORD(v0)` of an
  uninitialized 64-bit local and then tests the full 64-bit value — garbage
  upper half on arm64 alters control flow. Fixed under `#ifdef KISAK_IOS`
  with typed 32-bit locals reproducing the original x86 semantics;
  Windows `#else` byte-identical. (Unreached by the bounded proof path, but
  the TU is now linked on device; fail-honest beats fail-lucky.)
- Retained from the queued wave: `static_assert(sizeof(GfxCmdDrawTriangles)==24)`
  producer+consumer, `xyzwOffset = sizeof(GfxCmdDrawTriangles)` on iOS vs
  byte-preserved `16` on Win32, 64-bit byte-count math + INT16_MAX guards in
  the producer, declared-array-type indexing in `RB_DrawTriangles_Internal`.
- Struct spot-checks against arm64: `GfxVertex` = 32B with `color` at 16
  (r_gfx.h:642) ✓; `GfxCmdDrawTriangles` = 4+pad4+8+4+2+2 = 24B ✓;
  `GfxCmdArray` uses typed pointers ✓; `__rdtsc` → `cntvct_el0` shim exists ✓.

### Evidence bar mapping (this wave)

- (a) census 52/52 at the new floor — `ios-compile-probe.yml` dispatch.
- (b) device job: exact libkisakrenderer membership + symbol greps.
- (c) device job: nm of the Payload app binary shows the scene entry symbols.
- (d) sim job green with existing exact greps UNCHANGED; marker gains only the
  honest device-only status line; no new sim gate.
- Device RUNTIME (render= earned line, screenshot) is explicitly NOT claimed;
  it happens at the owner's sideload sitting; the CI gate for the earned line
  lands only with that evidence.

## Round 1 — Sol's challenges and answers

Sol standing: **NEEDS-FIX** (8 findings). Adjudication, challenge by
challenge; the plan is amended, not defended.

1. **BLOCKER — dropping c2bd68c drops the link-bounding mechanism. ACCEPTED
   and remedied.** Sol is right: c2bd68c's sim dead-strip was load-bearing,
   not simulator decoration — without dead stripping, extracted members carry
   undefined references to unported owners (verified:
   `R_ReadBspPreTessDrawSurfs` at r_draw_bsp.cpp:424/477 with owner
   r_pretess.cpp; `RB_DrawProfile`/`RB_DrawProfileScript` at
   rb_backend.cpp:1426-1427 with owner rb_drawprofile.cpp), and a no-dead-strip
   device link cannot close without a B2-scale scaffold wave. Adopted Sol's
   first remedy: `DEAD_CODE_STRIPPING[sdk=iphoneos*]: YES` with the Vulkan
   loader entry explicitly rooted (`-Wl,-u,_vkGetInstanceProcAddr`, the same
   rooting c2bd68c proved out in design for the sim) while keeping
   `-force_load`. The M12 lesson ("dead-strip removed vk*") predates the `-u`
   root; the config flip is honestly documented in project.yml and
   NEXT_SESSION.md, and the device job now nm-gates
   `_vkGetInstanceProcAddr`/`_vkCreateInstance`/`_vkCreateDevice` in the
   packaged IPA binary so a failed rooting is red in CI, not discovered at
   the owner's sideload sitting. Residual honest risk: the dlsym runtime
   behavior of this config is first proven on the iPad; recorded as such.
2. **MAJOR — retention predicate must include Clear's HRESULT. ACCEPTED.**
   Main discarded `dev->Clear`'s result; the predicate now captures `clearHr`
   and requires `SUCCEEDED(clearHr) && SUCCEEDED(readHr) &&
   pixel==0xFFBA55D3 && SUCCEEDED(presentHr)` before retaining the
   device/d3d9 pointers. The public smoke status string is byte-unchanged.
3. **MINOR — stale retention / repeat calls. ACCEPTED (documented one-shot).**
   The native attempt is one-shot per launch (`g_attempted`/`g_succeeded` in
   RendererPlaceholder.cpp); the bridge comment now states that repeat calls
   return the cached verdict of the single attempt, and the only Swift call
   site sits inside the one-shot d3d9 dispatch block. Retained pointers are
   never reassigned after the single smoke run (teardown ordering remains
   explicitly outside this bounded proof).
4. **MAJOR — "no new sim gate" was overbroad. ACCEPTED (reworded, kept).**
   Correct: making libkisakrenderer an exact required archive means a
   renderer TU that fails to compile for the *simulator* SDK now hard-fails
   the sim job's build step, where the census loop used to tolerate skips.
   This is a deliberate build-level strengthening — it is precisely the
   "simulator build must still COMPILE" coverage this wave owes — and it is
   the class of change "gates only strengthen" welcomes. The claim is
   restated as: no new sim *runtime/marker/symbol* gate; the sim workflow
   file is byte-identical; the strengthening is evidenced by the dispatch
   runs on this branch (queued never ran in CI, so first evidence is ours).
5. **MINOR — refusal marker is observation, not evidence; BridgingHeader is
   hunk-merged. ACCEPTED.** NEXT_SESSION.md now calls the sim `render=` line
   an expected artifact observation, not a gated assertion. BridgingHeader.h
   was hand-merged preserving main's `kisak_ff_kernel_smoke` declaration
   (a wholesale checkout would have deleted the FF path — good catch).
6. **MINOR — Sys_DestroySplashWindow graduation exception. ACCEPTED
   (documented).** The real owner (r_init.cpp) is an explicit, commented iOS
   no-op — "no Win32 splash window to destroy on iOS" — i.e. a
   behavior-matching real-minimal owner under the wave methodology, not a
   silent success path. The exception is documented at the scaffold site and
   in NEXT_SESSION.md rather than fighting the graduation rule.
7. **MINOR — minimality of hygiene + RB_AdaptiveGpuSyncFinal. PARTIALLY
   ACCEPTED.** The build-script hygiene (fresh OBJDIR, stale-archive rm,
   error propagation) is kept — a skipped renderer TU silently reusing
   yesterday's object is exactly the failure this wave cannot afford on the
   Mac day — but isolated in its own commit with rationale. The
   `RB_AdaptiveGpuSyncFinal` LP64 fix stays: the task mandates an LP64 sweep
   of every enabled TU, the function is now *linked* on device, and the
   uninitialized-upper-half read is a genuine arm64 control-flow hazard; the
   notes no longer describe rb_backend.cpp as "kept verbatim" but as "kept
   verbatim plus one swept fix". Windows `#else` remains byte-identical.
8. **NIT — floor wording. ACCEPTED.** The probe asserts a monotonic floor
   (`total >= 52`) plus `pass == total`; "52/52" describes the expected
   green outcome, not the assertion's shape.

## Round 2 — implementation review (Sol, ultra) and answers

Reviewed range: `6475ec7..f231181` (4 commits). Sol standing: **NEEDS-FIX**
(1 MAJOR, 3 MINOR, 1 NIT), with the round-1 remedy explicitly verified as
implemented. Verified-clean list from Sol: device dead-strip + `-u` root +
three-symbol Vulkan nm gates present; workflow working directories and
`proof-device/` paths correct; nm regexes match Darwin `ADDR T _symbol`
format; simulator workflow text byte-identical to `6475ec7`; `clearHr` in the
admission predicate; no duplicate/missing renderer symbols in either lane's
scaffold ownership; Swift/C++ success literals byte-identical (U+2014);
no path can emit the exact `render=` success marker without the full native
draw/stats/uploads/tess/readback/Present verification; Windows `#else`
bodies identical apart from EOF normalization; producer seam LP64-sound;
census exactly 52.

Findings, adjudicated — all five ACCEPTED and fixed in the round-2 commit:

1. **MAJOR — adaptive-sync spin timeout unreachable.** My round-1 rewrite
   kept `(__rdtsc() - startTime) < 0` where iOS `__rdtsc()` returns
   `unsigned long long` → unsigned comparison, always false,
   `RB_AbandonGpuFence()` dead. Fixed: explicit `uint32_t` wraparound delta
   (`(uint32_t)__rdtsc() - startTime`) with an `int32_t` sign test — the
   faithful 32-bit-register semantic. (Unreached by the bounded proof path,
   but the sweep's own claim demanded correctness.)
2. **MINOR — 64-bit delay decay truncated.** `dx.gpuSyncDelay` is
   `unsigned __int64` (r_init.h:306); making the whole temporary `uint32_t`
   narrowed the `127 * ((delay - 20000) / 0x80)` decay before the 64-bit
   store. Fixed: the decay computes in `unsigned long long` and stores full
   width; only the fence flag, spin delta, and the function's return value
   remain 32-bit register quantities, matching the original exactly.
3. **MINOR — caught-exception verdict not cached.** The bridge's catch paths
   returned strings the native `g_attempted` cache never records, so a
   hypothetical repeat call would report "not run" instead of the first
   exception verdict. Fixed: the bridge latches its FIRST verdict (including
   exception verdicts) in a static and returns it to any repeat caller; the
   detail pointer is likewise preserved by the early return.
4. **MINOR — unearned `render-detail=` could look earned.** The detail line
   was formatted before gate evaluation, so a failed `Present` could sit
   beside an earned-shaped detail line. Fixed: all gates evaluate first and
   an unearned attempt prefixes the detail with
   `NOT EARNED (see render= failure) `, which can never match the exact
   earned shape. (Sol confirmed the `render=` success marker itself was
   never forgeable; this hardens the secondary line to the same standard.)
5. **NIT — stale "simulator proof" comment** in r_init.cpp's `R_InitThreads`
   fence. Fixed to "bounded device placeholder proof".

Process note: round 2 ran twice — the first background invocation was
presumed dead after a session interruption (0 bytes of output at check time)
and a foreground retry was started per the empty-output rule; the original
then completed in full and the duplicate was cancelled before producing
output. The adjudicated text above is the original's complete verdict.
