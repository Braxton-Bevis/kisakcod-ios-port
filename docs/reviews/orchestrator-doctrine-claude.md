# Ruling on the ChatGPT "agent-only program orchestrator" doctrine — Claude (coordination seat)

*2026-07-14. The owner relayed a long orchestration doctrine authored by
ChatGPT without project context and delegated the adoption decision to the
coordination seat. Per the standing cross-review protocol: adopt on merit,
decline with reasons, record here.*

## ADOPTED

1. **Branch protection + mechanized promotion (its Gate 0 core).** `main`
   had no protection; promotion was a manual fast-forward gated only by
   coordinator discipline. Now: `main` is protected (required status
   checks on the exact SHA, enforce_admins, no force-push/deletes), and
   promotion goes through a staging→main pull request whose checks must
   pass. A semantic failure can no longer be promoted by hand.
2. **Evidence-tier vocabulary, applied with judgment.** The distinction
   SYNTAX / OBJECT / LINK / SIM_RUN / DEVICE_RUN is real and claims should
   name their tier. Applied: the compile probe is a SYNTAX gate; the stub
   lane is OBJECT+ARCHIVE+LINK+SIM_RUN for its archives; M12–M14 are
   DEVICE_RUN; slice-2 findings are container-level (Oracle 0) and
   REAL_ZONE_EVIDENCE.md already scopes them that way ("static pass",
   "not-observed ≠ proven-absent").
3. **Oracle taxonomy (0/1/2).** The current tool is Oracle 0 (container
   inspector). The FF0a *engine-instrumented* dump mode — already on the
   plan as the slice-7 exit criterion — is Oracle 1. Runtime asset-graph
   semantics must come from Oracle 1, not Oracle 0. Wording adopted.
4. **Layout conformance = ABI conformance.** The slice-6 result (zero
   numeric divergence, 17 structures / 155 members) is ABI-level evidence
   only; serialization semantics (counts, ownership, aliases, sentinels)
   are separate and remain open until oracle-driven. Already reflected in
   LAYOUT_CONFORMANCE.md; adopted as standing language.
5. **Gates must demonstrably be able to fail.** Already project practice
   (the layout gate went red on first contact before adjudication; the
   comparator self-test includes a negative case; markers are earned).
   Adopted as an explicit requirement for future gates.
6. **Diagnostic vs promotion workflow separation.** The staging
   continue-on-error softening + BMK4_VERDICT lines are the diagnostic
   class; protected-main required checks are the promotion class. The
   softening never applies to main. This formalizes the split that
   already existed.

## DECLINED (with reasons)

1. **The 12-file control plane, task-contract templates, and 20-role
   agent topology.** Right shape for a large program; wrong overhead for
   this one. The existing system of record (docs/NEXT_SESSION.md, the
   knowledge packs, docs/reviews/ cross-critiques, PORT_JOURNAL.md, the
   ratified ten-slice plan) already carries the same information with far
   fewer tokens. Revisit if the program grows lanes beyond the current
   coordinator+Sol structure.
2. **Re-running the plan as Gates 0–11.** The ten-slice plan was already
   converged through mutual adversarial review and ratified with
   amendments; its ordering embeds the same risk-retirement logic. The
   doctrine's genuinely new items (branch protection, oracle taxonomy,
   evidence tiers) are adopted above; the rest of its gate ladder
   duplicates the ratified slices.
3. **Architecture contest (direct translator vs pointer-free cache) as a
   blocking gate.** The pointer-free `.kac`-style cache is noted as a
   real alternative (independent macOS port precedent). But the ratified
   plan's translation kernel is oracle-gated and fixture-gated already;
   pausing for a two-prototype bake-off is not the highest risk retired
   per token today. The cache idea is recorded as a fallback architecture
   if the kernel's round-trip gates stall.
4. **Early physical-device sentries.** The owner's standing constraint is
   batched device sittings (Sideloadly, no Mac until unavoidable). The
   renderer path is already DEVICE_RUN-proven at M12. Sentry batching
   stays as ratified; the doctrine's schedule would burn owner time the
   program was explicitly told to conserve.
5. **"Do not manually fast-forward" as an absolute.** Adopted in effect
   via protection + PR promotion, but the reason is mechanization, not
   ceremony: a fast-forward of an exact SHA whose required checks are
   green on that SHA is equivalent evidence; the PR flow is simply what
   GitHub can enforce mechanically.

## Standing effects

- Promotion mechanism from now on: staging→main PR; required checks on
  `main` are the two Windows jobs (the always-running lane); iOS lanes
  are verified by the coordinator on the staging SHA before opening the
  PR (their path filters make them unenforceable as required checks
  without burning macOS minutes on every docs commit — revisit if minutes
  stop mattering).
- Claim language in README/docs names its evidence tier when a stronger
  reading is plausible.
- Sol lanes inherit rule 5 (gates must fail demonstrably) in their task
  briefs.
