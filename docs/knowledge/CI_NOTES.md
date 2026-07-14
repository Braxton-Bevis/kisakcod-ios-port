# CI notification and verdict notes

## Staging-only failure softening

BMK4 staging is discovery-driven: a red gate is useful evidence, but a failed
workflow-run conclusion sends the pushing actor an Actions email. Jobs that opt
into notification softening use this job-level property:

```yaml
continue-on-error: ${{ github.ref == 'refs/heads/staging' }}
```

Every original command, assertion, gate, and default step condition stays
unchanged. A failed step therefore keeps its real `failure` outcome and later
steps keep their normal `skipped` outcomes. Only a job on the exact staging ref
is allowed to report success at job/run level. On `main` the expression is
false, so failures remain hard failures and notifications behave exactly as
before.

Each softened job also gives every existing step a stable `id` and ends with a
step named `Verdict for coordinator`:

```yaml
- name: Verdict for coordinator
  if: github.ref == 'refs/heads/staging' && always()
```

That final step examines the ordered `steps.<id>.outcome` values. It prints and
adds to `$GITHUB_STEP_SUMMARY` exactly one machine-readable line:

```text
BMK4_VERDICT=green
BMK4_VERDICT=red step=<first non-successful step name>
```

`build-kisarcod-win.yaml` emits one verdict per matrix job, so Debug and Release
must be read independently.

## Coordinator watcher contract

Do not treat a successful staging run conclusion as proof that its gates passed.
Read every staging job's verdict from the log:

```sh
gh run view RUN_ID --log | grep 'BMK4_VERDICT='
```

PowerShell equivalent:

```powershell
gh run view RUN_ID --log | Select-String 'BMK4_VERDICT='
```

If logs are unavailable, inspect the published per-step conclusions (the API
representation of the recorded outcomes):

```sh
gh run view RUN_ID --json jobs --jq '.jobs[] | {name, conclusion, steps: [.steps[] | {name, conclusion}]}'
```

For staging, a coordinator may advance only when every relevant job has
`BMK4_VERDICT=green`, or equivalently every original gate step has conclusion
`success`. A softened run-level `success` by itself is never a green signal.

## `ios-stub.yml` recipe for its next holder

Do not change its assertions or commands. Apply the pattern independently to
both jobs:

1. Add `continue-on-error: ${{ github.ref == 'refs/heads/staging' }}` beside
   `runs-on` in `simulator-launch-proof` and `device-ipa-unsigned`.
2. Assign these IDs, in the existing order:
   - `simulator-launch-proof`: `checkout`, `toolchain_versions`,
     `build_engine_archives`, `install_xcodegen`, `generate_xcode_project`,
     `build_simulator`, `record_phase3_ownership`, `simulator_launch_proof`,
     `upload_simulator_proof`.
   - `device-ipa-unsigned`: `checkout`, `build_renderer_engine_archives`,
     `install_xcodegen`, `generate_xcode_project`, `build_device`,
     `verify_package_ipa`, `upload_unsigned_ipa`.
3. Append `Verdict for coordinator` as the final step of each job with
   `if: github.ref == 'refs/heads/staging' && always()`.
4. Map every ID into that step's `env` using
   `OUTCOME_<ID>: ${{ steps.<id>.outcome }}`. In the same order as the job,
   select the first value other than `success`, print
   `BMK4_VERDICT=red step=<display name>`, and append the same line to
   `$GITHUB_STEP_SUMMARY`; if all values are `success`, print and append
   `BMK4_VERDICT=green`.

Use this shell body after populating an ordered `checks` array with entries in
the form `"$OUTCOME_<ID>|<display name>"`:

```bash
verdict='BMK4_VERDICT=green'
for check in "${checks[@]}"; do
  outcome=${check%%|*}
  step=${check#*|}
  if [[ "$outcome" != success ]]; then
    verdict="BMK4_VERDICT=red step=$step"
    break
  fi
done
echo "$verdict"
echo "$verdict" >> "$GITHUB_STEP_SUMMARY"
```

The upload steps must be included in the checks. Keep the verdict step last and
do not put `continue-on-error` on individual gate steps.

## Residual email sources

- Workflow-file syntax or schema errors occur before any job starts. Job-level
  `continue-on-error` and the verdict step cannot run, so GitHub can still send
  a failure email.
- The open staging-to-main PR creates `pull_request` runs whose `github.ref` is
  a synthetic `refs/pull/<number>/merge`, not `refs/heads/staging`. The exact
  staging-ref expression above intentionally does not soften those runs. Close
  that coordination PR when it is not needed, or obtain owner approval for a
  separate PR-event policy.
- The only total mute is the user-side GitHub setting at
  **Settings -> Notifications -> Actions -> Email**. That also hides meaningful
  main-branch and pre-job failures, so it is a broader tradeoff than this
  repository-side pattern.
