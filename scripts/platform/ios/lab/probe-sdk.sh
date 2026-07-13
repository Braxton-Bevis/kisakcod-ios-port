#!/usr/bin/env bash
set -euo pipefail

out=${BMK4_LAB_OUT:-lab-out}
mkdir -p "$out"

{
  echo '=== macOS ==='
  sw_vers
  uname -a
  echo
  echo '=== Xcode ==='
  xcode-select -p
  xcodebuild -version
  echo
  echo '=== Installed SDKs ==='
  xcodebuild -showsdks
  echo
  for sdk in iphoneos iphonesimulator macosx; do
    echo "=== $sdk ==="
    xcrun --sdk "$sdk" --show-sdk-platform-path
    xcrun --sdk "$sdk" --show-sdk-path
    xcrun --sdk "$sdk" --show-sdk-version
  done
} > "$out/sdk-info.txt" 2>&1

xcrun simctl list runtimes > "$out/simulator-runtimes.txt"
xcrun simctl list runtimes --json > "$out/simulator-runtimes.json"
xcrun simctl list devicetypes --json > "$out/simulator-device-types.json"
