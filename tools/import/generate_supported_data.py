#!/usr/bin/env python3
"""Generate tools/import/supported_data.v1.json from docs/ASSET_INVENTORY.md.

The manifest is the machine-readable "exactly these files, exactly these
hashes" contract for the first-playable data set (frontier-plan ruling,
docs/reviews/frontier-plan-claude-ruling.md). Hashes only — game files never
enter git/CI/artifacts/logs. Run from the repo root.
"""

import io
import json
import re

REQUIRED_ZONES = {
    "code_post_gfx_mp.ff",
    "localized_code_post_gfx_mp.ff",
    "ui_mp.ff",
    "common_mp.ff",
    "localized_common_mp.ff",
    "mp_killhouse.ff",
    "mp_killhouse_load.ff",
}

ROW = re.compile(r"\| (.+?) \| (\d+) \| ([0-9a-f]{64}) \|")


def wanted(path: str) -> bool:
    lower = path.lower()
    if lower.startswith("main/") and lower.endswith(".iwd"):
        return True
    if lower == "localization.txt":
        return True
    if lower.startswith("zone/english/"):
        return lower.split("/", 2)[2] in REQUIRED_ZONES
    return False


def main() -> None:
    rows = []
    with io.open("docs/ASSET_INVENTORY.md", encoding="utf-8") as fh:
        for line in fh:
            match = ROW.match(line)
            if match:
                path = match.group(1).replace("\\", "/")
                rows.append((path, int(match.group(2)), match.group(3)))

    required = [
        {"path": path, "bytes": size, "sha256": digest}
        for (path, size, digest) in rows
        if wanted(path)
    ]
    manifest = {
        "schema": "bmk4.supported-data.v1",
        "edition": "Call of Duty 4: Modern Warfare — Steam PC (App 7940), English",
        "generated_from": "docs/ASSET_INVENTORY.md (owner install, 2026-07-13)",
        "notes": (
            "Hashes only; game files never enter git/CI/artifacts/logs. "
            "This is the exact file set the first-playable contract accepts; "
            "verification rejects unknown or modified files before any deep "
            "parsing."
        ),
        "required": sorted(required, key=lambda row: row["path"]),
    }
    with io.open("tools/import/supported_data.v1.json", "w", encoding="utf-8") as fh:
        fh.write(json.dumps(manifest, indent=2) + "\n")
    print(f"required files: {len(required)}")


if __name__ == "__main__":
    main()
