# MAC DAY PLAN — first Mac sitting, zero wasted minutes

*Written 2026-07-16 (coordination seat), pre-critique draft; amendments from
the Sol review of `docs/reviews/pre-mac-plan-claude.md` fold in here. This is
the ordered checklist for the first session on a physical Mac (or a
long-lived cloud Mac with GUI). Everything here is EITHER Mac-only or
dramatically cheaper on a Mac. Anything doable from the Windows seat stays
off this list.*

## Preconditions (verify before starting the clock)

- [ ] Xcode ≥ 16 installed, iOS SDK present, simulator runtime downloaded.
- [ ] Homebrew: `meson ninja glslang xcodegen` (same set CI installs).
- [ ] Repo cloned; `git log origin/main -1` matches the SHA this plan was
      last updated against.
- [ ] The iPad + USB cable on hand IF device items are in scope for the
      sitting (they can be a separate sitting).
- [ ] NO game assets on the Mac. Everything here is asset-free by design.

## Item 1 — D1 null-descriptor patch validation (highest Mac-only value)

The one written-but-never-compiled renderer blocker fix. Full procedure in
`docs/knowledge/DXVK_D1_NULL_DESCRIPTOR.md` (build script outline + the
engine-like unbound-slot probe + acceptance gate). Summary:

1. Clone DXVK v2.7.1 shallow; apply `scripts/platform/ios/dxvk-v2.7.1-ios.patch`
   then `scripts/platform/ios/dxvk-v2.7.1-ios-null-descriptor.patch`
   (`git apply --check` first — exact chain already verified).
2. Build via `scripts/platform/ios/build-dxvk-ios.sh`; assert `libdxvk_d3d9.a`
   arm64 + `Direct3DCreate9` exported.
3. Run the unbound-slot probe (D3D9Smoke variant, precompiled vs_2_0/ps_2_0
   DWORD arrays, texture slot 3 deliberately unbound, ps samples s3):
   `MVK_CONFIG_DEBUG=1 DXVK_LOG_LEVEL=debug DXVK_WSI_DRIVER=iOS`.
4. Acceptance: MoltenVK log confirms nullDescriptor absent; DrawPrimitive
   D3D_OK; center pixel == marker color ±1 LSB (unbound sample read zero);
   no validation errors/device loss. Record accept/reject + logs.
5. On ACCEPT: commit the evidence note; the slice-8 asset waves inherit a
   validated renderer base.

## Item 2 — Renderer on a real Apple GPU (settles the Lane 1 question)

The hosted CI simulator GPU (paravirtual) lacks
`shaderSampledImageArrayDynamicIndexing` (+ likely the descriptorIndexing
family). A local Mac's simulator uses the host M-series GPU and may expose
them. Ten minutes settles it:

1. Build + run the stub app in the LOCAL simulator from the staging DXVK
   plumbing (`f5d854c` lineage). Check `vulkaninfo`-equivalent from MoltenVK
   logs: does the local sim device report the missing features?
2. If YES: the sampler-fallback lane is a hosted-CI-only concern — decide
   whether to keep it (CI regression value) or park it; run the
   `renderer-placeholder-queued` scene locally → real-GPU placeholder
   screenshot immediately.
3. If NO: the fallback wave stays load-bearing for any simulator proof;
   proceed with it as planned.
4. Either way: build the placeholder scene DIRECT TO THE IPAD via Xcode
   (personal team signing) → DEVICE_RUN placeholder screenshot — the first
   real-GPU engine-path render artifact. Honest label: generated placeholder
   scene, real IW3 R/RB → D3D9 → DXVK → Vulkan → MoltenVK → Metal path.

## Item 3 — Batched physical-device checkpoints (needs the iPad)

In ONE install session (Sideloadly from Windows also works if the Mac
sitting lacks the iPad):

- [ ] M13/M14 physical feel-test markers (deferred since 2026-07-13).
- [ ] M15 on device: the full headless boot marker set on real hardware
      (upgrades M15 from SIM_RUN to DEVICE_RUN).
- [ ] Placeholder-scene frame on device (Item 2.4 if not already done).
- [ ] Export marker files via the share sheet; screenshots; journal.

## Item 4 — Xcode-debug anything device-only

Whatever the backlog holds by Mac day: crashes reproducible only on
hardware, Metal validation-layer runs of the DXVK path
(`MTL_DEBUG_LAYER=1`), Instruments GPU capture of the placeholder scene
(baseline for slice-8 perf sanity).

## Item 5 (stretch) — Slice-8 scouting

- Attempt `R_Init` end-to-end with shader_bin/precompiled-blob path against
  the D1-validated DXVK; record where it stops. Every symbol it demands is
  slice-8 wave intel.

## No-Mac fallback (if the sitting slips)

Items 2.4 and 3 do not require a Mac at all: CI's unsigned IPA + Sideloadly
on the Windows laptop + a ~30-minute owner iPad sitting produce the
DEVICE_RUN placeholder screenshot and the batched markers. Only Items 1, 4,
and 5 genuinely need the Mac.
