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

  # Leaf subset the stub app links today (see ios/Stub/EngineSmoke.cpp): TUs
  # whose engine-extern closure is small enough for the documented scaffolding.
  local smoke=() missing=0
  for leaf in src_universal_com_memory.cpp.o src_universal_dvar.cpp.o \
              src_qcommon_cmd.cpp.o src_universal_com_math.cpp.o \
              src_universal_q_shared.cpp.o src_qcommon_msg_mp.cpp.o \
              src_qcommon_huffman.cpp.o src_ios_msvc_crt_compat.cpp.o; do
    if [ -f "$OBJDIR/$leaf" ]; then
      smoke+=("$OBJDIR/$leaf")
    else
      echo "ERROR ($sdk): required smoke object missing: $leaf" >&2
      missing=1
    fi
  done
  [ "$missing" -eq 0 ] || return 1
  xcrun libtool -static -o "ios/libs/$sdk/libkisaksmoke.a" "${smoke[@]}" 2>/dev/null
  echo "[$sdk] smoke subset (${#smoke[@]} TUs) -> ios/libs/$sdk/libkisaksmoke.a"
}

case "$WHICH" in
  iphoneos)        build_one_sdk iphoneos        arm64-apple-ios15.0 || exit 1 ;;
  iphonesimulator) build_one_sdk iphonesimulator arm64-apple-ios15.0-simulator || exit 1 ;;
  both)            build_one_sdk iphoneos        arm64-apple-ios15.0 || exit 1
                   build_one_sdk iphonesimulator arm64-apple-ios15.0-simulator || exit 1 ;;
esac
