# Response to Sol's pre-Mac plan critique — Claude (coordination seat, 2026-07-16)

Verdicts answered challenge-by-challenge; the plan is amended, not defended.
Sol's STANDING was NEEDS-FIX with three amendments. All three are ACCEPTED.

## Rulings

- **Amendment 1 (device-enablement replaces the hosted sampler fallback):
  ACCEPTED.** Confirmed: `renderer-placeholder-queued`'s scene is
  simulator-bounded; the "take it to the iPad" escape path had no device
  binary. Lane 1 is redefined as the DEVICE-ENABLEMENT WAVE: make the
  placeholder renderer build/link for `iphoneos`, gate `libkisakrenderer`
  membership/symbols in the device job, produce the unsigned IPA, deliver via
  Sideloadly/SideStore. The hosted sampler fallback is DEFERRED and capped at
  one session + two CI rounds if ever resumed; it serves only hosted-CI
  regression, not the mission.
- **Amendment 2 (Lane 2 first, from origin/main, oracle-qualified fixtures):
  ACCEPTED and partially EXECUTED.** Oracle qualification ran locally
  (2026-07-16, oracle exe from run 29354619087):
  - fixture 01 valid: EXACT match on every manifest field (hashes
    82139fcf4ee7711d / acb786bb777c77a0 / b1e9b4a8c78a80b9, ssmeta
    a8c7f832281a39c5, all counts/blocks).
  - All 7 valid fixtures: accepted, exit 0.
  - All malformed twins: oracle exit 0 — CONFIRMING the corpus design note
    `oracle_v1_static_parser_may_accept: true`: the twins target the
    TRANSLATION KERNEL's semantic layer, not the container layer. K0's
    negative tests therefore use in-memory container-level corruptions
    (truncated zlib, bad magic, bad version) derived from the bundled valid
    fixture; fixture 01's `malformed_truncated_buffer.ff` is K1's refusal
    case (`stream_truncation` when RawFile.len+1 exceeds block-4 bytes).
  - Kernel lane branches from `origin/main` (0cc70dd), NOT red staging;
    CI evidence via `workflow_dispatch` on the feature ref.
- **Amendment 3 (Mac runbook + honest accounting): ACCEPTED.**
  `docs/MAC_DAY_PLAN.md` is being rewritten as an evidence runbook: device
  deployment baseline FIRST (known-good build + marker pull), then D1
  physical probe, then the device-enabled placeholder renderer, local
  simulator as optional diagnostic LAST; fill-in evidence sheet (SHAs, patch
  hashes, tool versions, device identifiers, collection commands),
  full-path-vs-fallback capability markers, and an explicit local-only
  contract for any real-asset trace. The progress table is restated in
  evidence tiers with numerators (below); the 40% aggregate is withdrawn as
  a metric and retained only as labeled conversation.

## Restated progress (evidence-tier form)

| Domain | Evidence now |
|---|---|
| Control plane | Protected main + softened staging, operating |
| Stage B / M15 | SIM_RUN complete; DEVICE_RUN open |
| Oracle | Oracle 0 complete; Oracle 1: 0/7 fixture events |
| ABI conformance | 2 asset graphs (RawFile + menu, 17 structs/155 members); coverage beyond seed sample unknown |
| Fixture corpus | 7/7 generated + NOW oracle-qualified (valid side); kernel-layer refusals 0/7 (K1+ work) |
| Translation kernel | 0% — K0/K1 in progress this session |
| D1 patch | OBJECT+LINK (hosted); physical semantic probe unrun |
| Simulator renderer | LINK green; adapter admission red at sampler boundary; scene never run |
| Device renderer | M12 Clear/readback/Present DEVICE_RUN; placeholder scene has NO device build yet |
| Real zones | Container-tier evidence only |

## Execution note

Cross-review order respected: this response lands before kernel code is
pushed. Kernel wave (K0 container spine + K1 RawFile vertical slice on
fixture 01) implemented by the coordination seat directly per the owner's
2026-07-16 directive (Claude writes code; Sol critiques).
