#!/bin/bash
# Build libkisakcod.a for iOS (device and/or simulator) from the census file
# list in scripts/ios/CMakeLists.txt. TUs that fail to compile are skipped
# with a report — the archive automatically grows as the census set graduates.
#
# Usage: build-engine-lib.sh <dxvk-native-include-dir> [iphoneos|iphonesimulator|both]
# Output: ios/libs/<sdk>/libkisakcod.a
set -uo pipefail

DXVK_NATIVE=${1:?usage: build-engine-lib.sh <dxvk>/include/native [sdk]}
WHICH=${2:-both}
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$ROOT"

FILES=$(sed -n 's/^ *"\${SRC_DIR}\/\(.*\)".*$/src\/\1/p' scripts/ios/CMakeLists.txt)

build_one_sdk() {
  local sdk=$1 target=$2
  local SDKPATH; SDKPATH=$(xcrun --sdk "$sdk" --show-sdk-path)
  local OBJDIR="build-ios-lib/$sdk"
  mkdir -p "$OBJDIR" "ios/libs/$sdk"
  local ok=0 skip=0 objs=()
  for f in $FILES; do
    local o="$OBJDIR/$(echo "$f" | tr '/' '_').o"
    if xcrun clang++ -target "$target" -isysroot "$SDKPATH" \
         -std=gnu++20 -c -O1 -ferror-limit=5 \
         -fno-strict-aliasing -fwrapv -fms-extensions -fdelayed-template-parsing \
         -w -Wno-c++11-narrowing \
         -DKISAK_MP -DKISAK_IOS -Isrc -Ideps \
         -isystem "$DXVK_NATIVE/directx" -isystem "$DXVK_NATIVE/windows" \
         "$f" -o "$o" 2>"$OBJDIR/last_err.log"; then
      objs+=("$o"); ok=$((ok+1))
    else
      echo "SKIP ($sdk): $f — $(grep -m1 ' error: ' "$OBJDIR/last_err.log" | cut -c1-100)"
      skip=$((skip+1))
    fi
  done
  xcrun libtool -static -o "ios/libs/$sdk/libkisakcod.a" "${objs[@]}" 2>/dev/null
  echo "[$sdk] archived $ok TUs (skipped $skip) -> ios/libs/$sdk/libkisakcod.a"
  lipo -info "ios/libs/$sdk/libkisakcod.a"
}

case "$WHICH" in
  iphoneos)        build_one_sdk iphoneos        arm64-apple-ios15.0 ;;
  iphonesimulator) build_one_sdk iphonesimulator arm64-apple-ios15.0-simulator ;;
  both)            build_one_sdk iphoneos        arm64-apple-ios15.0
                   build_one_sdk iphonesimulator arm64-apple-ios15.0-simulator ;;
esac
