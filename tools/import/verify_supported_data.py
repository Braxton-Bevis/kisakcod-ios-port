#!/usr/bin/env python3
"""Verify a user-owned COD4 install against supported_data.v1.json.

Platform-neutral verifier — the same logic the future in-app importer runs
before copying/converting anything. Prints one line per required file plus a
summary; exits 0 only when every required file is present with the exact
size and SHA-256. Never copies, uploads, or logs game-file CONTENT — paths,
sizes, and hashes only.

Usage:
  python tools/import/verify_supported_data.py "D:\\SteamLibrary\\steamapps\\common\\Call of Duty 4"
"""

import hashlib
import io
import json
import os
import sys


def sha256_of(path: str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    root = sys.argv[1]
    manifest_path = os.path.join(os.path.dirname(__file__), "supported_data.v1.json")
    with io.open(manifest_path, encoding="utf-8") as fh:
        manifest = json.load(fh)

    ok = missing = mismatched = 0
    for entry in manifest["required"]:
        rel = entry["path"]
        path = os.path.join(root, rel.replace("/", os.sep))
        if not os.path.isfile(path):
            print(f"MISSING   {rel}")
            missing += 1
            continue
        size = os.path.getsize(path)
        if size != entry["bytes"]:
            print(f"SIZE      {rel} expected={entry['bytes']} actual={size}")
            mismatched += 1
            continue
        digest = sha256_of(path)
        if digest != entry["sha256"]:
            print(f"HASH      {rel}")
            mismatched += 1
            continue
        print(f"OK        {rel}")
        ok += 1

    total = len(manifest["required"])
    print(f"\nsummary: {ok}/{total} verified, {missing} missing, {mismatched} mismatched")
    print(f"edition: {manifest['edition']}")
    return 0 if ok == total else 1


if __name__ == "__main__":
    sys.exit(main())
