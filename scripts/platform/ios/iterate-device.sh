#!/bin/bash
# One full DXVK→iPad iteration: rebuild dxvk, restage, rebuild app, deploy,
# launch, pull marker + dxvk stderr, print the verdict lines.
set -uo pipefail
export PATH="$HOME/Library/Python/3.9/bin:$HOME/.local/bin:$PATH"
DEV=21B43CC9-D658-522F-A4A7-72EF38CE29B8
SCRATCH=/tmp/bmk4-work

cd /Users/tracybevis/dxvk
ninja -C build-ios | tail -1 || exit 1
cp build-ios/src/dxvk/libdxvk.a build-ios/src/d3d9/libdxvk_d3d9.a \
   build-ios/src/util/libutil.a build-ios/src/wsi/libwsi.a \
   build-ios/src/vulkan/libvkcommon.a build-ios/src/dxso/libdxso.a \
   /Users/tracybevis/kisakcod-ios-port/ios/libs/iphoneos/

cd /Users/tracybevis/kisakcod-ios-port/ios
xcodebuild -project KisakStub.xcodeproj -scheme KisakStub -configuration Debug \
  -destination "platform=iOS,id=$DEV" -derivedDataPath build/dd \
  -allowProvisioningUpdates build 2>&1 | grep -E " error:|BUILD " | head -4
grep -q "BUILD SUCCEEDED" <(xcodebuild -project KisakStub.xcodeproj -scheme KisakStub -configuration Debug -destination "platform=iOS,id=$DEV" -derivedDataPath build/dd build 2>&1 | tail -3) || true

xcrun devicectl device install app --device $DEV build/dd/Build/Products/Debug-iphoneos/KisakStub.app 2>&1 | grep bundleID
xcrun devicectl device process launch --device $DEV dev.braxton.kisakstub 2>&1 | tail -1
sleep 12
xcrun devicectl device copy from --device $DEV --domain-type appDataContainer \
  --domain-identifier dev.braxton.kisakstub --source Documents/metal_first_frame.txt \
  --destination "$SCRATCH/iter_marker.txt" 2>/dev/null | tail -1
echo "--- marker:"
grep -E "d3d9=|engine=" "$SCRATCH/iter_marker.txt" || true
xcrun devicectl device copy from --device $DEV --domain-type appDataContainer \
  --domain-identifier dev.braxton.kisakstub --source Documents/dxvk_stderr.log \
  --destination "$SCRATCH/iter_stderr.log" 2>/dev/null | tail -1
echo "--- dxvk log tail:"
grep -E "Skipping|required|D3D9|Presenter|adapter|Direct3D|err:|warn:" "$SCRATCH/iter_stderr.log" | tail -10
