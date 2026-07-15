# BMK4 — Full handoff to GPT 5.6 Codex (2026-07-15)

You are taking over as **lead engineer + coordinator** for the BMK4 project.
The previous lead (Claude Fable) is out of credits. This document is
everything you need to continue without rediscovering context. Read it fully
before touching anything. When in doubt, the durable source-of-truth files
named at the end override this summary.

---

## 0. THE ONE THING TO DO FIRST (uncommitted, unverified work in the tree)

`git status` right now shows **uncommitted B4 (slice 4) work** left by the
previous Sol lane, which was **killed before it reported or was audited**:

- New files: `src/ios/sys_ios_events.cpp` (real `Sys_QueEvent`/`Sys_GetEvent`
  ring), `ios/Stub/BootEventSmoke.cpp` (the event-loop proof stage).
- Modified: `src/qcommon/common.cpp` (B3→B4 boundary extension),
  `src/universal/dvar_cmds.cpp` (real `set`/`seta` owners graduating),
  `src/qcommon/qcommon.h`, `ios/Stub/BootScaffold.cpp` (new `Scr_MonitorCommand`
  scaffold; note `sys_ios_main.mm` lost ~114 lines — verify what graduated),
  `scripts/ios/CMakeLists.txt` (census +2: `sys_ios_events.cpp`,
  `dvar_cmds.cpp`), `scripts/platform/ios/build-engine-lib.sh` (cominit_members
  +2), `ios/Stub/BootComInit.cpp`, `ios/Stub/MetalViewController.swift`,
  `ios/Stub/BridgingHeader.h`.

**This code has NEVER compiled or linked in CI. Do not trust it.** Audit it as
adversarially as if a stranger wrote it (it effectively was — an interrupted
agent). Then: commit → push to **staging** → watch CI → fix red rounds → only
promote to main when fully green. See §6 for the exact gate. Details of what
B4 must prove are in §8 (slice 4).

If the audit finds the work incoherent or half-baked, it is legitimate to
`git checkout -- .` / `git clean` the B4 files and re-implement slice 4 from
scratch per §8 — a clean redo beats debugging a mystery half-diff.

---

## 1. Mission & the goal artifact (frozen)

Port **KisakCOD** (a GPL-3.0 source reconstruction of the Call of Duty 4:
Modern Warfare multiplayer engine, by LWSS) to **arm64-apple-ios** so it runs
on a **physical iPad**. The single frozen deliverable:

> An **in-game screenshot of `mp_killhouse` running on the physical iPad**,
> posted to the public GitHub repo. NOT a menu screenshot. The frame must come
> from the real engine path (fastfile load → renderer), not a synthetic mockup.

Repo: **https://github.com/Braxton-Bevis/bmk4** (public, GPL-3.0). Owner:
Braxton (GitHub Braxton-Bevis). Note the repo was renamed from
`kisakcod-ios-port` to `bmk4`; the git remote is already updated locally.

---

## 2. Environment & hard constraints

- **Dev machine is Windows 11** with **NO local C/C++ compiler and no Xcode.**
  You cannot build the engine locally. **All compile/link/runtime verification
  happens in GitHub Actions CI.** The only things that run locally are: git,
  gh CLI, Python (3.12), and the Windows-built `bmk4-ff-oracle.exe` tool.
- Shell is **PowerShell 5.1** (primary) or Git Bash. Beware PS pitfalls:
  native stderr under `$ErrorActionPreference='Stop'` becomes a terminating
  `NativeCommandError`; embedded quotes in `git commit -m` mangle messages
  (use a heredoc / `@'...'@` literal blocks); `exit $LASTEXITCODE` leaks
  intended non-zero exit codes.
- **Line endings:** the repo has a `.gitattributes` pinning `*.patch`, `*.sh`,
  `*.layout` to `eol=lf`. Windows working-copy shows CRLF warnings on commit —
  harmless, the blobs are stored LF. Do NOT let apply-sensitive files (patches,
  shell scripts) get CRLF-committed.
- **The physical iPad and any Mac are last-resort.** The whole plan is designed
  to be Mac-free and device-free until the very end. Device installs go via
  **Sideloadly on the Windows laptop** (signs CI's unsigned IPA with the
  owner's Apple ID over USB; 7-day resign; reinstall over-the-top, never
  uninstall). The owner only physically operates the iPad at the final sitting.

---

## 3. ASSET RULE (legal — never violate)

The owner legally owns COD4 on Steam, installed at
`D:\SteamLibrary\steamapps\common\Call of Duty 4`. **Proprietary game files
(`.ff`, `.iwd`, textures, models, audio, strings) must NEVER enter git, CI,
build artifacts, model prompts, or logs.** Only hashes, counts, sizes, and
pathnames may be committed. Real-asset processing happens **locally only**
(e.g. the oracle tool runs on the Windows laptop; only sanitized structural
reports are committed). When you need something from real assets, produce a
sanitized report. `docs/ASSET_INVENTORY.md` (168 files hashed) and
`docs/REAL_ZONE_EVIDENCE.md` (six boot/killhouse zones, structural only) are
examples of the allowed form.

---

## 4. Git flow (HARD RULE — mechanized 2026-07-15)

1. **NEVER push directly to `main`.** `main` has branch protection: required
   status checks = the two "Build with DirectX SDK (Debug/Release)" jobs,
   `enforce_admins` on, no force-push/deletion. A direct push is refused.
2. **All work pushes to `staging`.** Watch CI on staging.
3. **Promotion = a staging→main pull request** with auto-merge:
   `gh pr create --base main --head staging ...` then
   `gh pr merge <N> --auto --merge`. It merges itself when the required checks
   pass. `allow_auto_merge=true` is set on the repo.
4. Open the PR only when the **iOS lanes are green on the staging SHA too**
   (they're not required checks because their `paths:` filters would make them
   flaky as required — so you verify them by eye before opening the PR).
5. Rationale: earlier, direct-to-main pushes re-triggered CI on a broken main
   and spammed the owner with GitHub failure emails. The staging softening
   (§6) plus this PR flow ended that.

Current SHAs (2026-07-15): `main` = `08fc489` (merge of staging through
`8f833b7`), `staging` HEAD = `8f833b7`. The uncommitted B4 work is on top of
`8f833b7`.

---

## 5. Roles & how the previous coordination worked

Previously: **Claude Fable** = lead/coordinator/adversarial-reviewer;
**ChatGPT Sol** (`gpt-5.6-sol`, driven via `codex exec` / `codex exec resume`)
= implementer. They ran a **mutual adversarial cross-review** protocol (each
critiques the other's work/plans; findings in `docs/reviews/`).

**Now you (Codex) are lead.** You can be both implementer and self-reviewer,
but preserve the *discipline* that made this work: separate the act of writing
a change from the act of verifying it. When you implement something, audit it
with fresh-eyes skepticism before committing — the project's quality came from
never trusting a claim without checking git state and CI evidence. If the
owner later re-enables a second model (Claude, or a second Codex lane), resume
the `docs/reviews/` cross-review format.

The Sol implementer sessions were driven with:
`codex exec resume -c model_reasoning_effort="high" -c sandbox_mode="workspace-write" -c sandbox_workspace_write.network_access=true <session-id>`.
That sandbox **cannot reach git/gh credentials**, so the coordinator always did
the real commit + push + CI watch. `danger-full-access` sandbox was DENIED by
policy — do not retry it. (As solo lead you may not need sub-sessions at all.)

---

## 6. The wave methodology (how every slice is built & gated)

This is the core engineering discipline. Follow it exactly:

- **Linker-driven waves.** You add real engine translation units (TUs) to the
  iOS build until the linker stops complaining. Each **undefined symbol** the
  linker reports tells you the next TU to bring in. You bring in 5–10 TUs per
  wave, not the whole engine.
- **The census** (`scripts/ios/CMakeLists.txt`, `IOS_PROBE_SOURCES`) is the
  monotonic list of TUs that must compile for arm64-iOS. It **only grows**.
  Currently **37/37** (about to become 39 with B4's two additions). The compile
  probe CI (`ios-compile-probe.yml`) asserts `total >= N` and `pass == total`;
  raise the floor when you add TUs.
- **Scaffolds** (`ios/Stub/BootScaffold.cpp`): for symbols whose real owner TU
  isn't in the wave yet, you write a stand-in. Two legal kinds:
  (a) **abort-loud** — `BOOT_UNREACHED("SymbolName")` prints the symbol and
  aborts, so if the boot ever reaches it you see exactly what's missing;
  (b) **real-minimal / behavior-matching** — when the real function is a no-op
  or trivially reproducible on the headless path (with a comment citing the
  real owner file:line and the wave that must delete it).
  **Never fake success.** A scaffold that pretends work happened is a lie that
  produces a hollow marker. Unknown states fail closed (abort), never guess.
- **Graduation discipline:** when a real owner TU joins the census, you MUST
  **delete its scaffold** from `BootScaffold.cpp` (and any placeholder globals
  in `EngineSmoke.cpp`), or the linker reports **duplicate symbols**. Check
  with `nm` before/after. This bit us repeatedly in the B3 chain — expect it.
- **Markers are EARNED, never hardcoded.** The app writes a marker line to
  `Documents/metal_first_frame.txt` **only after** the real subsystem
  behaviorally succeeds (e.g. the `net=` marker is written only after a packet
  survives encode→loopback→decode). CI greps for the exact marker line. A
  Swift-side or hardcoded fake must not be able to satisfy the gate.
- **Gates only strengthen.** Once a marker/assertion is in CI, it stays
  forever. Add new assertions; never weaken existing ones.
- **`#ifdef KISAK_IOS` / `#else` byte-identical Windows lanes.** Every engine
  edit is fenced so the Windows build is byte-for-byte unchanged. The Windows
  CI (`build-kisarcod-win.yaml`) proves this on every push. Never change
  Windows behavior without a separate deliberate regression test.
- **LP64 vigilance.** The KisakCOD source is a **32-bit x86 decompilation**. It
  carries hidden 32-bit-pointer assumptions. The B3 crash was exactly this:
  `huffman.cpp` sorted an array of *pointers* with a hardcoded 4-byte element
  size and did pointer math on `uint32_t` — fine on Win32, a segfault on arm64
  (pointers are 8 bytes). **Proactively sweep any code you wire for:**
  `qsort` with literal element sizes, `*(uint32_t*)` casts of pointers,
  `int`/`__int16` truncation of pointers or handles, struct layouts assuming
  4-byte pointers. Fix under `#ifdef KISAK_IOS` with typed code; keep Win32
  byte-identical in `#else`.

### The staging CI softening (why red on staging ≠ email spam)

On the `staging` branch, the iOS jobs and the Windows job use
`continue-on-error: ${{ github.ref == 'refs/heads/staging' }}` and emit a
`BMK4_VERDICT=green|red step=<name>` line (and layout emits
`BMK4_LAYOUT_CONFORMANCE=green|red`) into the step summary. So a run shows
"success" at the GitHub level even when a step failed — **you must read the
per-step outcomes, not just the run conclusion.** On `main` and PRs the jobs
hard-fail normally (that's what protects main). This is the diagnostic-vs-
promotion split. Recipe details in `docs/knowledge/CI_NOTES.md`.

### How to watch CI (the loop the previous lead ran)

```
gh run list --branch staging --limit 8 --json databaseId,workflowName,headSha,status,conclusion
# for each run on your SHA, pull per-step outcomes because of the softening:
gh run view <id> --json jobs   # inspect .jobs[].steps[].conclusion != success/skipped
# on a red step, get the error:
gh run view <id> --log-failed   # grep for 'error:', 'Undefined symbols', 'duplicate symbol', 'SIGSEGV'
# download proof artifacts (crash evidence, marker file):
gh run download <id> --name simulator-launch-proof --dir <scratch>
```

The three workflows: `build-kisarcod-win.yaml` (Windows, the required checks,
also builds the FF oracle + runs the layout conformance gate),
`ios-compile-probe.yml` (arm64 syntax census, macOS runner),
`ios-stub.yml` (builds engine archives, runs the app in the iOS **simulator**,
captures the marker file + screenshots + `.ips` crash reports, and builds the
unsigned device IPA).

---

## 7. Milestones so far (M = "Milestone", journaled in `PORT_JOURNAL.md`)

M0 env check · M1 stub app renders · M2 compile census · M3 FS sandbox ·
M4 D3D9 header wall solvable · M6 stub v2/v3 (MetalFX, controllers, settings) ·
M7 reproduced on a borrowed Mac · M8 census 23/23 · **M9 DXVK builds as an iOS
lib** · M10 engine code executes on iOS · M11 first run on the physical iPad ·
**M12 renderer live: first D3D9 frame DXVK→Vulkan→MoltenVK→Metal on iPad,
pixel-exact** · M13 staged engine boot (memory/dvar/command, simulator) ·
M14 real `bg_pmove` on a synthetic floor (simulator) · **M15 IN PROGRESS** =
headless `Com_Init`: filesystem ✓, **network loopback ✓ (just done)**, event
loop (B4, the uncommitted work) next, then the M15 closeout marker.

(Watch for the "M5" ambiguity: "iPad Pro 13-inch (M5)" and "Apple M5 GPU" refer
to the iPad's **chip**, not a milestone.)

---

## 8. The ratified ten-slice plan & where we are

Authoritative order: `docs/reviews/roadmap-sol.md` as amended/ratified in
`docs/reviews/roadmap-claude-response.md`. Superseded stage doc:
`docs/ROADMAP_TO_PLAYABLE.md`. Status:

1. **FF0a Windows oracle instrument** — ✅ DONE (tool built, CI-green).
2. **Real-zone evidence run** (local only) — ✅ DONE. All six screenshot-path
   zones parse clean; **blocks 2/3/5/6 carry zero bytes in every zone** (kernel
   scope shrank; delay-stream glue likely off the critical path). FNV anchors
   recorded. See `docs/REAL_ZONE_EVIDENCE.md`.
3. **B3 net/msg loopback** — ✅ DONE (M15 part 2). `net=` marker earns in CI;
   Huffman LP64 crash fixed. Promoted to main in PR #4.
4. **B4 Com_EventLoop probe** — 🔄 **UNCOMMITTED IN TREE (see §0).** Frozen
   gate: a console command **queued through the engine's real event queue**
   must **observably change a dvar**, read back through the dvar system — a
   Swift fake cannot satisfy it. Plus a **negative test** in the same stage: an
   invalid/garbage command must NOT change the dvar and the process must
   survive. `dvar_cmds.cpp` (real `set`/`seta` owners) graduates here. Target
   marker (verify against what Sol actually wrote):
   `event=Com_EventLoop OK — queued console event set bmk4_b4_probe=alive, invalid cmd rejected`.
   After it earns in CI, wire the exact marker into `ios-stub.yml` (gates only
   strengthen) and raise the census floor.
5. **B5 M15 closeout** — ⬜ the marker `cominit=Com_Init OK — 4 subsystems up,
   no assets`, dvar count > 24, behavioral set/dvarlist, FS write/read/delete,
   event-loop probe. Retires M15 and Stage B. Journal it in `PORT_JOURNAL.md`.
6. **OAT layout-conformance gate** — ✅ DONE. Engine-derived struct layouts vs
   OpenAssetTools manifests: **zero numeric divergence** (17 structures / 155
   members); the four flagged diffs were naming-only union differences,
   adjudicated (`docs/LAYOUT_CONFORMANCE.md`). Gate is HARD in Windows CI now.
   This is **ABI conformance only** — serialization semantics are still open
   (that's slice 7). `tools/layout_conformance/`.
7. **Synthetic FF fixture kernel** — fixtures ✅ DONE (`tools/zone_fixtures/`:
   7 mechanisms × valid+malformed twin, deterministic builder, per-fixture
   manifests). The **translation kernel itself is the dominant remaining
   cost**: iOS FF1 load spine (header/block/zlib, byte-identical to oracle),
   then FF2 translation core (32→64 pointer widening driven by layout maps),
   round-trip-gated against the oracle on the synthetic zones then real zones.
8. **Renderer + asset waves** — D1 DXVK dummy-resources patch ✅ WRITTEN
   (`scripts/platform/ios/dxvk-v2.7.1-ios-null-descriptor.patch`,
   `docs/knowledge/DXVK_D1_NULL_DESCRIPTOR.md`; static-verified only, needs a
   Mac-lane compile+probe). Then asset-type waves (material→techset→image→
   xmodel→…), `R_Init`, first engine-driven simulator frame → swap README hero
   image to that real render.
9. **mp_killhouse direct-client closure** — clipmap/gfxworld load, real
   collision, minimal touch input, launch via
   `+set sv_pure 0 +devmap mp_killhouse` (local listen server, offline), full
   client links → unsigned IPA.
10. **Device Day** — Sideloadly install, copy game files into the app's
    Documents (= fs_homepath) via the Windows "Apple Devices" app, batched
    proofs, **the killhouse screenshot → posted to GitHub.**

**Critical path to the artifact:** B4 → B5 (M15 done) → slice 7 translation
kernel (the hard middle) → slice 8 render → slice 9 killhouse → slice 10 device.

---

## 9. Key file & location reference

- **Boot orchestration (iOS stub app):** `ios/Stub/` — `MetalViewController.swift`
  (drives the boot stages, writes marker lines), `BootComInit.cpp`,
  `BootNetSmoke.cpp` (B3 proof), `BootEventSmoke.cpp` (B4, new), `EngineSmoke.cpp`,
  `BootScaffold.cpp` (the scaffold graveyard — abort-loud + real-minimal
  stand-ins with deletion boundaries).
- **Engine source:** `src/` — `qcommon/common.cpp` (the `Com_Init` spine; the
  KISAK_IOS headless boundary lives in `Com_Init_Try_Block_Function` ~line
  1350), `qcommon/net_chan_mp.cpp`, `qcommon/huffman.cpp`, `win32/win_net.cpp`
  (BSD sockets + loopback policy under KISAK_IOS), `universal/dvar*.cpp`,
  `universal/com_memory.cpp`, `ios/sys_ios_*.{mm,cpp}` (iOS platform layer),
  `universal/q_shared.*` (LittleShort/etc. no-ops + `__rdtsc` seam).
- **iOS build glue:** `scripts/ios/CMakeLists.txt` (the census list),
  `scripts/platform/ios/build-engine-lib.sh` (builds archives; `cominit_members`
  is the exact required Com_Init archive), `scripts/platform/ios/preflight-lint.sh`
  (MSVC-ism scanner — run its patterns mentally before committing engine edits).
- **Workflows:** `.github/workflows/` — `build-kisarcod-win.yaml`,
  `ios-compile-probe.yml`, `ios-stub.yml`.
- **Knowledge packs (READ THESE for the domain):** `docs/knowledge/` —
  `FF_RUNTIME_NOTES.md` (fastfile format, triangulated from 3 sources),
  `RENDERER_INIT_NOTES.md`, `CI_NOTES.md` (softening recipe + watcher),
  `DXVK_D1_NULL_DESCRIPTOR.md`.
- **Plan/state:** `docs/NEXT_SESSION.md` (per-wave live state — UPDATE THIS as
  you go), `PORT_JOURNAL.md` (milestone journal — append M15 when it closes),
  `docs/reviews/` (cross-review mailbox + the ratified roadmap + the
  orchestrator-doctrine ruling), `docs/ROADMAP_TO_PLAYABLE.md`.
- **Fastfile format facts (confirmed on retail data):** magic `IWffu100`,
  version 5, 44-byte XFile header, 9 blocks named
  temp/runtime/large_runtime/physical_runtime/virtual/large/physical/vertex/
  index, pointer token `0xffffffff`, `decompressed_bytes == xfile.size + 44`.
  Boot zone load order: code_post_gfx_mp → localized_code_post_gfx_mp → ui_mp →
  common_mp → localized_common_mp → mod.

---

## 10. The renderer path (for slice 8 context)

engine D3D9 calls → **DXVK** (patched to build for iOS, CAMetalLayer WSI) →
**Vulkan** → static **MoltenVK 1.4.1** → **Metal**. Proven live at M12
(Clear/readback/Present pixel-exact on the iPad). The one known blocker is
**D1**: MoltenVK 1.4.1 lacks `VK_EXT_robustness2`/`nullDescriptor`, but real
COD4 shaders leave descriptor slots unbound and stock DXVK writes
`VK_NULL_HANDLE` there (illegal without that feature). The D1 patch routes
unbound slots to persistent dummy resources, gated on the device not exposing
`nullDescriptor`. It applies cleanly on top of the base
`scripts/platform/ios/dxvk-v2.7.1-ios.patch` and is static-verified; it needs a
first compile + an asset-free sparse-binding probe on a Mac lane before you
trust it (plan in `DXVK_D1_NULL_DESCRIPTOR.md`).

Public claim on the README (fact-checked): "first publicly documented app
calling DXVK's D3D9 frontend directly on physical iOS/iPadOS." Prior art
credited: Ammaar Reshi's C&C Generals port (D3D8 via DXVK's D3D9-backed impl).
Don't overstate it.

---

## 11. Evidence-tier vocabulary (adopted 2026-07-15)

Name a claim's evidence tier; don't inflate adjectives. Tiers: SOURCE / SYNTAX
(the census `-fsyntax-only` pass) / OBJECT / ARCHIVE / LINK / SIM_RUN (behaves
in simulator) / DEVICE_RUN (behaves on the physical iPad) / ORACLE_SYNTH /
ORACLE_REAL_LOCAL / STABILITY. The compile probe is SYNTAX. The stub lane is
OBJECT+ARCHIVE+LINK+SIM_RUN. M12/M13(device parts)/M14 device claims are
DEVICE_RUN. Slice-2 findings are container-level (call the oracle an "Oracle 0
container inspector", not "the answer key"). Layout conformance is ABI-level,
not serialization. Simulator success ≠ device success. A screenshot supports
but never replaces logs+hashes+behavioral checks. Full ruling on what was
adopted/declined from the owner's big "orchestrator doctrine":
`docs/reviews/orchestrator-doctrine-claude.md`.

---

## 12. Known pitfalls already hit (don't repeat)

- Duplicate symbols when a TU graduates but its scaffold/placeholder isn't
  deleted (`Com_Filter`, `Com_SafeMode`, `_copyDWord`, `msg_dumpEnts`,
  `msg_printEntityNums`). Always `nm`-diff after graduating.
- Missing system-header includes after a scaffold rewrite (`unistd.h` for
  `sysconf`). The census is `-fsyntax-only`; the stub-app **link** finds more.
- LP64 pointer bugs in decompiled code (the Huffman `qsort` — §6).
- The oracle refusal test & PS `ErrorActionPreference` interactions (see the
  FF0a step in `build-kisarcod-win.yaml` for the guarded pattern).
- `.git/index.lock` contention if you run parallel write lanes — serialize
  commits; the coordinator commits on agents' behalf.
- Don't count "boxes done" as progress; measure risk retired. The hard middle
  (slice 7 translation kernel) dominates and is still ahead.

---

## 13. Immediate next actions for you, in order

1. **Audit the uncommitted B4 work** (§0). Read every changed file. Check:
   scaffolds deleted for graduated TUs (`dvar_cmds.cpp` was previously
   scaffolded? — grep BootScaffold/EngineSmoke), the `common.cpp` B4 boundary
   is `#ifdef KISAK_IOS` fenced and Windows-identical, `BootEventSmoke.cpp`
   reads the dvar back through the real dvar system (not a fake) AND has the
   negative test, LP64-swept the event-ring code. The `sys_ios_main.mm` −114
   lines needs explaining — confirm what graduated and that no scaffold now
   duplicates it.
2. If sound: **commit** (message crediting the source; end with the
   `Co-Authored-By` trailer the owner uses), **push to staging**, **watch CI**.
   Expect a linker-wave red or two — fix them the wave way (§6).
3. When the B4 round is fully green (read per-step outcomes!), **wire the exact
   `event=` marker assertion** into `ios-stub.yml`, bump the census floor in
   `ios-compile-probe.yml`, push, re-verify, then **open the staging→main
   promotion PR** (§4) and arm auto-merge.
4. **Then slice 5 (B5):** the M15 closeout marker. Then journal M15 in
   `PORT_JOURNAL.md` and update `docs/NEXT_SESSION.md`.
5. After M15: begin **slice 7** (the FF translation kernel) against
   `tools/zone_fixtures/` and the oracle — this is the long pole.
6. Keep `docs/NEXT_SESSION.md` current every wave; it's the live state of
   record if you're interrupted like the last lane was.

Welcome to the seat. The methodology is the moat — trust the linker, earn the
markers, fence the Windows lane, and never let an adjective outrun its
evidence.
