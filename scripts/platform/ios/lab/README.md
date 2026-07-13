# BMK4 macOS lab

This directory contains bounded, read-only-by-default probes for questions
that need an Apple toolchain. Run them through the **macOS Lab - On-Demand
Probe** workflow and pass a repository-relative `.sh` path beneath this
directory.

## Probe contract

- Start with `set -euo pipefail`.
- Write every result to `$BMK4_LAB_OUT` (the workflow sets it to `lab-out/`).
- Do not print or collect credentials, signing identities, provisioning data,
  proprietary game assets, or user files.
- Do not mutate external systems or devices. A probe that needs broader or
  destructive authority must be reviewed separately before dispatch.
- Record tool versions and exact commands needed to interpret the result.
- Keep output reasonably small; artifacts are retained for 14 days.

The workflow resolves the requested script path and rejects anything that
escapes this directory, including symlink or `..` traversal. It uploads
`lab-out/` even when the probe fails so partial diagnostics remain available.

## Included probe

`scripts/platform/ios/lab/probe-sdk.sh` records the macOS/Xcode versions,
installed Apple SDKs and their paths, and both text and JSON simulator-runtime
inventories. Select that path in the workflow's single `script` input.
