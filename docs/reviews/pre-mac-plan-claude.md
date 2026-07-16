# Pre-Mac execution plan — Claude (coordination seat, back from quota reset, 2026-07-16)

*Context: Codex (Sol, full-access seat) ran the project 2026-07-15 and closed
B4+B5/M15, then pushed the renderer toward the owner's "screenshot ASAP"
directive until the hosted simulator GPU's `shaderSampledImageArrayDynamicIndexing`
boundary. Checkpoint of record: `docs/NEXT_SESSION.md` at staging `f5d854c`.
This plan covers the remaining Windows-seat work BEFORE the first Mac sitting,
and the Mac sitting itself. Submitted for one adversarial critique round to
Sol per the cross-review protocol.*

## Assessment of the 2026-07-15 run (for the record)

Accepted as excellent: B4 (real queued console events, permanently gated,
PR #5) and B5 (M15 closed: `cominit=Com_Init OK — 4 subsystems up, no assets`,
72 dvars, behavioral set/dvarlist/FS/event/script-strings, 41/41 census,
PR #6 → main `0cc70dd`). Stage B is retired. Discipline held under pressure:
red staging was NOT promoted, no clear-frame was published as "the render",
markers stayed earned, evidence tiers stayed named. The `shaderInt64`
two-uint32 rewrite instead of a capability lie is exactly the house style.

One strategic observation (not a defect): the simulator renderer push fought
four hosted-simulator GPU capability gaps with a fifth (sampler indexing +
descriptorIndexing family) still ahead. **The physical iPad has none of these
gaps** — M12 proved CreateDevice/Clear/readback/Present pixel-exact on real
hardware through the same DXVK chain. The simulator renderer is a CI
regression-gate convenience, not the goal path. That reframes how much more
we spend on it (see Lane 1 time-box).

## Ground state (verified 2026-07-16)

- `main` = `0cc70dd` (M15 + gates; fully green, protected).
- `staging` = `f5d854c` (simulator-DXVK plumbing; green everywhere EXCEPT the
  simulator D3D smoke, which fails closed at adapter admission — the hosted
  GPU lacks `shaderSampledImageArrayDynamicIndexing`).
- `renderer-placeholder-queued` = `5121928` (census→51, generated room scene
  through real IW3 `R_GetCommandBuffer(RC_DRAW_TRIANGLES)` → `RB_*` → D3D9;
  audited, NEVER run in Apple CI).
- Local variant `renderer-placeholder-impl` (`6dd1b2b`) differs from queued
  only in the DXVK base patch (−83 lines); canonical is the REMOTE queued
  branch per the checkpoint doc. The local variant is reconciled/discarded in
  Lane 0.
- Stale remote branches: `mac-bringup` (M6-era), `codex/phase2-pmove-ci` —
  archive/delete candidates, zero urgency.
- Slice-7 inputs all staged and green: oracle tool, real-zone anchors, ABI
  layout maps + hard CI gate, 7×(valid+malformed) deterministic fixtures.

## The plan — four lanes before the Mac

### Lane 0 — Housekeeping (coordinator, same day)
1. Sync local `main`/`staging` refs to remote; retire the two scratch
   worktrees once their branches are reconciled.
2. Reconcile `renderer-placeholder-impl` vs `renderer-placeholder-queued`:
   diff is confined to `dxvk-v2.7.1-ios.patch`; keep whichever patch variant
   the green staging run `29438330184` actually built, discard the other.
3. Update `docs/NEXT_SESSION.md` to point at this plan; push docs to staging.

### Lane 1 — Simulator sampler fallback, TIME-BOXED (screenshot lane)
Implement the coherent fallback wave exactly as the checkpoint designed it:
bounded fixed/constant-index sampler path when
`shaderSampledImageArrayDynamicIndexing` is absent, safe descriptor-set
creation without descriptorIndexing/partially-bound/runtime-array,
`samplerMirrorClampToEdge` emulation-or-refusal, abort-loud for any textured
engine shader the fallback cannot honestly reproduce, full physical-device
path preserved via capability gating.

- Gate 1: unchanged CreateDevice/Clear/readback/Present marker green in the
  simulator.
- Gate 2: rebase `renderer-placeholder-queued`; land its link-closure commit,
  then its scene commit; require native draw-count + non-background readback +
  Present markers; visually inspect the artifact.
- Then: publish the screenshot honestly labeled "generated placeholder scene
  through the real IW3 R/RB→D3D9→DXVK path (simulator)", promote to main,
  add a README progress section (hero stays reserved for the real frame).
- **TIME-BOX / STOP RULE: if one more load-bearing missing-feature wall
  appears behind the sampler wave (a sixth boundary), halt Lane 1 and carry
  the renderer proof to real Apple hardware (Mac sitting or Sideloadly iPad
  sitting) instead.** The emulated GPU is not the mission; the iPad is.

### Lane 2 — Slice 7 kernel start (the long pole; fully Mac-independent)
Begin immediately, in parallel, as the PRIMARY lane by priority:
1. FF1 load spine on iOS: `.ff` header parse, block allocation, zlib inflate —
   gated by byte-identical FNV hashes vs the oracle on fixture 01
   (rawfile valid + malformed twin refused). Fixtures are synthetic and CI-legal.
2. FF2 translation core: staging buffer at `Load_Stream`, layout-map-driven
   32→64 fills, `block<<28|offset` −1-bias pointer resolution, −1/−2 sentinel
   branches — round-trip vs oracle manifests on fixtures 01–02, then 03–07.
3. Windows lane stays byte-identical; every mechanism keeps its malformed
   twin refusing (fail-closed).
This lane decides the real killhouse screenshot's date; nothing about it
needs a Mac or the simulator renderer.

### Lane 3 — Mac-day plan (docs now, so the sitting wastes zero minutes)
Write `docs/MAC_DAY_PLAN.md` with the ordered checklist:
1. **D1 validation** (the highest-value Mac-only item): compile DXVK with
   `dxvk-v2.7.1-ios-null-descriptor.patch`, run the asset-free unbound-slot
   probe per `docs/knowledge/DXVK_D1_NULL_DESCRIPTOR.md`, record accept/reject.
2. **Renderer on real Apple GPU**: run the placeholder scene on the local
   simulator (an M-series host may expose the features the hosted paravirtual
   GPU lacks — settles whether Lane 1's gap is hosted-runner-only) and build
   direct to the iPad via Xcode for a DEVICE_RUN placeholder screenshot.
3. Batch physical checkpoints: M13/M14 device markers, M15 on device.
4. Xcode-debug anything device-only discovered by then.
5. Timebox scouting for slice-8: capture a real `R_Init` trace if feasible.
Also documented: the no-Mac fallback — Sideloadly + the unsigned IPA can put
the placeholder scene on the iPad without any Mac, in one ~30-minute owner
sitting.

## Progress rating (risk-retired, by domain)

| Domain | Status |
|---|---|
| Control plane / CI truthfulness | ~100% (protected main, earned markers, softened staging) |
| Startup (Stage B / M15) | 100% — CLOSED |
| Windows oracle (Oracle 0) | 100% of its tier; Oracle 1 instrumentation still open |
| ABI layout conformance | 100% (hard gate); serialization semantics 0% (slice 7) |
| Fixtures / kernel inputs | 100% staged |
| Translation kernel (slice 7) | 0% — THE dominant remaining risk |
| Renderer, device path | M12-proven for Clear/Present; D1 patch written, unvalidated |
| Renderer, simulator path | ~80% (one designed feature-wave from green) |
| Real zones / assets | Container-level evidence done |
| Device/final artifact | IPA pipeline proven; killhouse frame far |

Aggregate: **~40% of total risk to the killhouse artifact retired.** The next
30% lives almost entirely in Lane 2.

## Questions Sol should attack

1. Is the Lane 1 time-box right, or should the simulator renderer be dropped
   NOW in favor of device/Mac proof (is a hosted-CI-provable placeholder
   screenshot worth one more feature wave)?
2. Is starting Lane 2 with FF1-on-fixture-01 the correct smallest first
   kernel slice, or is there a smaller/differently-shaped first cut?
3. Does anything in this plan risk Windows-lane byte-identity or the asset
   rule?
4. Is the Mac-day ordering right (D1 first vs renderer-on-real-GPU first)?
5. What is being forgotten?
