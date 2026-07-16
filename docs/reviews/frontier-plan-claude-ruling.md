# Ruling on the frontier-model completion plan — Claude (coordination seat, 2026-07-16)

*The owner relayed a long static-review/acceleration plan from a frontier
model WITHOUT code-execution access, reviewed at main 6475ec7, and asked for
a critical read. Verdict: the strongest outside review the project has
received — it found one CONFIRMED P0 DEFECT our own two-model review loop
missed — mixed with a minority of recommendations that misread project state
it could not see. Rulings below; the owner has endorsed following its broad
order, and this ruling operationalizes that.*

## CONFIRMED P0 DEFECT it caught (accept immediately)

**The kernel's 64 MiB decompressed-payload cap refuses the goal artifact's
own zones.** Our own docs/REAL_ZONE_EVIDENCE.md records
localized_common_mp = 70,269,481 bytes and mp_killhouse = 76,935,387 bytes
decompressed — both above the cap I wrote into ff_kernel.cpp and defended as
"explicit policy" one review round ago. Neither Sol round cross-referenced
the real-zone table. ACCEPTED with its improved fix shape, which is also
more engine-faithful than the current doubling realloc:
header-first exact allocation — inflate the first 44 bytes, read xfile.size,
checked-add +44, validate against a policy bound derived from the zone
manifest, allocate exactly once, continue the SAME zlib stream, require
exact final length + end-of-stream + no trailing bytes. Assigned to Lane A
(owns ff_kernel.cpp in the K2 wave). The K0 marker gate must be extended to
prove a >64MiB synthetic zone loads (gates must fail: and a
beyond-policy zone refuses).

## Accepted (high value, scheduled)

1. **Host-native `libbmk4_asset` build** (§9/§10 seed): compile the kernel
   TU for Windows/macOS hosts as a library + test driver. This is the
   single highest-leverage unscheduled item: local iteration without CI
   rounds, ASan/UBSan, fuzzing of the container/pointer decoder, and —
   decisive — LOCAL REAL-ZONE runs against the oracle on the owner's
   machine. Scheduled as its own wave right after the current lanes land.
2. **First-playable contract** (§2): adopted nearly verbatim as the slice
   9/10 acceptance contract (12-point same-process checklist, 10-minute
   stability, proof bundle; screenshot = evidence, not acceptance). Will
   land as docs/FIRST_PLAYABLE_CONTRACT.md at next integration.
3. **Census ≠ closure ≠ app split** (§3): accepted directionally. The
   `bmk4_killhouse_closure` fixed-manifest link target (no skips, no
   undefined symbols, no app-side fakes of required engine functions)
   becomes the slice-9 gate. The census stays a porting metric and stops
   being quoted as completion.
4. **Zone-context ownership + structured errors** (§6/§14): extends Sol's
   challenge-2 finding from generation-binding (shipped) to full owned
   context with transactional translate→commit. Adopted as the K2/K3
   refactor shape, matching the dispatch-table walker Lane A just built.
5. **Interval map for interior pointers** (§7): adopted as the K3 design
   basis (fixtures 04/05 need exactly this; a start-pointer dictionary is
   provably insufficient for interior references).
6. **PR-sequence** (§17): adopted as the refined roadmap framing, including
   its key sequencing rule — zones/killhouse waves (PR15+) do not merge
   while renderer D1/D2/R_Init (PR12–14) are unresolved. This matches the
   ratified plan's renderer/asset interleave and hardens it.
7. **README drift** (41/41 vs 42 floor): fix at next integration; adopt
   status generation (docs/status.json) as tooling debt, not a blocker.
8. **DXVK 3.0.1 branch matrix** (§11): accepted as an EXPERIMENTAL lane
   only (new shader compiler + SM1-3 path is relevant); 2.7.1 + D1 patch
   remains the baseline; no migration on recency alone. MoltenVK still
   lacks nullDescriptor, so D1 stays load-bearing regardless.
9. **Diagnostic micro-maps via IW3xRadiant** (§11 D4): adopted for slice
   8/9 — debug render features on purpose-built tiny maps, not inside
   mp_killhouse first.
10. **Reproducibility hygiene** (§4): quickstart path fix, DEVELOPMENT_TEAM
    out of the committed project (Local.xcconfig), dependency lock manifest,
    embedded build manifest — progressive adoption starting next
    integration.

## Rejected or corrected (it could not see project state)

1. **"OAT has effectively passed; make it a codegen dependency."** Conflates
   ABI-layout conformance (our shipped gate: 17 structs/155 members agree)
   with ZoneCodeGenerator ADOPTION (our spike found templates are
   hard-registered in CodeGenerator.cpp; extensibility is the blocker). The
   ratified stance stands: dual-derivation layout gates now, KisakCOD-header
   generation primary, OAT as cross-check; ZCG adoption re-evaluated at the
   material wave where struct counts explode.
2. **Milestone-archive consolidation now** (§3): premature. The exact-
   membership milestone archives ARE the proof harness that keeps waves
   honest. Consolidation into one engine archive happens WITH the
   killhouse-closure target (its own PR 4 position), not before.
3. **"Stop broadening isolated demonstrations"**: already converged — the
   running lanes ARE its recommendation (Lane B = its Renderer Gate D1/D2
   path; Lane C = its sanitized oracle trace §13; Lane A = its §5-§7 kernel
   corrections). No redirection needed.
4. **Desktop importer/product sections** (§15/§16): correct and far
   downstream; parked unchanged.
5. Minor: its evidence-tier instinct duplicates the adopted doctrine
   (docs/reviews/orchestrator-doctrine-claude.md); labels merge into ours.

## Operational effects (next integration cycle)

- Lane A brief extended: header-first exact-size container reader replaces
  the 64 MiB cap + doubling realloc; K0 gate grows a >64MiB synthetic
  accept + beyond-policy refuse pair.
- New wave queued after lanes land: host-native libbmk4_asset + sanitizers
  + fuzz seed corpus + local real-zone differential vs oracle.
- docs/FIRST_PLAYABLE_CONTRACT.md + README drift fix + quickstart/team-ID
  hygiene ride the next docs integration.
- The PR-sequence table becomes the roadmap's execution frame, mapped onto
  the ratified ten slices (they nest cleanly).
