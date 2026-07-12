#!/bin/bash
# Local reproduction of .github/workflows/ios-compile-probe.yml census step.
set -uo pipefail
cd /Users/tracybevis/kisakcod-ios-port
SDK=$(xcrun --sdk iphoneos --show-sdk-path)
DXDIR=/Users/tracybevis/dxvk/include/native/directx
WINDIR=/Users/tracybevis/dxvk/include/native/windows
LOGS=/tmp/bmk4-work/census/logs
mkdir -p "$LOGS"
FILES=$(sed -n 's/^ *"\${SRC_DIR}\/\(.*\)".*$/src\/\1/p' scripts/ios/CMakeLists.txt)
printf "%-32s %-6s %s\n" "file" "status" "first_error"
for f in $FILES; do
  name=$(echo "$f" | tr '/' '_')
  if xcrun clang++ -target arm64-apple-ios15.0 -isysroot "$SDK" \
       -std=gnu++20 -fsyntax-only -ferror-limit=25 \
       -fno-strict-aliasing -fwrapv -fms-extensions -fdelayed-template-parsing -w -Wno-c++11-narrowing \
       -DKISAK_MP -DKISAK_IOS -Isrc -Ideps \
       -isystem "$DXDIR" -isystem "$WINDIR" \
       "$f" > "$LOGS/$name.log" 2>&1; then
    printf "%-32s %-6s %s\n" "$f" "PASS" "-"
  else
    first=$(grep -m1 ' error: ' "$LOGS/$name.log" | cut -c1-160 || true)
    printf "%-32s %-6s %s\n" "$f" "FAIL" "${first:-unknown}"
  fi
done
