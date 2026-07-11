# PORT_JOURNAL — KisakCOD → arm64-apple-ios capability probe

Experiment log. Format per entry: **Attempted / Concrete change / Compiled-Ran? / Exact error / Next hypothesis.**
Goal hierarchy: (1) iOS pipeline, (2) build retarget + dependency map, (3) FS sandboxing, (4) renderer via TRANSLATION (D3D9→DXVK→Vulkan→MoltenVK→Metal), (5) touch input.
This is a capability experiment; partial, well-documented progress is the success criterion.

---

## M0 — Environment reality check (2026-07-11)

**Attempted:** Establish what this development seat (Windows 11 laptop) can and cannot do for an iOS target.

**Findings (verified by direct probes):**

| Capability | Status | Evidence |
|---|---|---|
| Xcode / xcodebuild / xcrun | ❌ absent | `Get-Command` — not on PATH; Xcode does not exist for Windows |
| Visual Studio / MSVC | ❌ absent | `vswhere.exe` missing — no VS install on this machine |
| clang / cmake / ninja | ❌ absent | not on PATH |
| git | ✅ | `C:\Program Files\Git\cmd\git.exe` |
| GitHub CLI, authenticated | ✅ | `gh auth status` → account **Braxton-Bevis**, scopes incl. `repo` |
| Apple Mobile Device Service | ✅ running (iTunes drivers) | `Get-Service` |
| iPhone attached right now | ❌ | PnP entries present but status `Unknown` (historical) |
| Local iOS SDK | ❌ | ships only with Xcode |

**Consequence — pipeline architecture:** every compile, link, sign, and launch step for iOS
must run on a **GitHub Actions macOS runner** (free Xcode + iOS SDK + Simulator). This
Windows seat authors code + project spec + workflows, pushes, and verifies runner output
(logs, screenshots, artifacts). The user has confirmed they will move to a real Mac for
finalization; everything here must therefore be reproducible from a clean `git clone`.

**On-device launch scoping:** a *physical* device launch requires an Apple ID signing
identity (interactive) + attached iPhone. Neither is available to the agent. Achievable
proxy, in decreasing strength: (a) CI builds the app and **launches it in an iOS
Simulator, with screenshot + in-app sandbox marker file as proof**; (b) CI emits an
**unsigned device .ipa** artifact + a documented Sideloadly (Windows) / Xcode (Mac)
signing runbook for the human last-mile. Both are in scope for M1. Physical launch is
logged as a HUMAN-STEP, not attempted-and-faked.

**Repo baseline (KisakCOD-master snapshot, 29.5 MB / 1034 files):**
- CMake ≥3.16, three targets: `KisakCOD-sp`, `KisakCOD-mp`, `KisakCOD-dedi` (scripts/{sp,mp,dedi}).
- Win32/x86 only: `cmake -G "Visual Studio 17 2022" -A Win32`, MSVC flags (`/MT /O2 ...`), DirectX SDK 2010 (D3D9).
- Platform override mechanism already exists: `scripts/platform_override.cmake` swaps
  `src/<path>` for `src/_platform/<platform>/<path>` per file. **`scripts/platform/linux/platform.cmake`
  is an empty placeholder** — no Linux/macOS port exists; mechanism is usable for an `ios` platform.
- Proprietary Win32-x86-binary deps linked into mp: **binklib (Bink video), msslib (Miles Sound System), steamsdk**.
  No ARM64/iOS versions exist → must be stubbed on iOS.
- Existing CI: `.github/workflows/build-kisarcod-win.yaml` (Windows, triggers on `master` only).
  New repo uses default branch `main` so it stays dormant until deliberately invoked.

**Next hypothesis:** a minimal XcodeGen-defined Swift+CAMetalLayer app, built and
simulator-launched entirely in CI, is achievable in one milestone with zero engine code.

---
