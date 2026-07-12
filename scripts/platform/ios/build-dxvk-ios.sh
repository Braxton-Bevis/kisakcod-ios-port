#!/bin/bash
# Build DXVK's d3d9 module as a static library for arm64-apple-ios.
#
# This is FRONTIER_REPORT "what a human does next" #2, resolved 2026-07-11:
# dxvk v2.7.1 + the 5-hunk patch next to this script (dxvk-v2.7.1-ios.patch)
# produces libdxvk_d3d9.a (arm64, Direct3DCreate9/Ex exported) with no other
# changes. The walls that were feared simply did not exist — DXVK's "native"
# (non-Windows) build is POSIX-clean; the patch is: __APPLE__ in the
# win32-compat gate, an lzcnt overload cast, Apple's 1-arg pthread_setname_np,
# a missing <cstddef>, and static-archive-instead-of-version-script linking.
#
# Prerequisites (all obtainable without root):
#   - Xcode + iOS SDK            (xcode-select -p must resolve)
#   - meson + ninja              (python3 -m pip install --user meson ninja)
#   - glslang                    (KhronosGroup/glslang releases, osx binary, on PATH)
#   - cmake                      (cmake.org binary tarball, on PATH)
#
# Usage: build-dxvk-ios.sh <workdir>   (defaults to ./dxvk-ios-work)
set -euxo pipefail

WORK=${1:-dxvk-ios-work}
HERE=$(cd "$(dirname "$0")" && pwd)
SDK=$(xcrun --sdk iphoneos --show-sdk-path)
MIN_IOS=15.0
mkdir -p "$WORK" && cd "$WORK"

# --- SDL2 for iOS (DXVK-native's WSI backend; official iOS support upstream) ---
if [ ! -f sdl2-ios/lib/libSDL2.a ]; then
  [ -d SDL ] || git clone --depth 1 --branch release-2.32.10 https://github.com/libsdl-org/SDL
  cmake -S SDL -B SDL/build-ios -GNinja \
    -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=$MIN_IOS \
    -DSDL_STATIC=ON -DSDL_SHARED=OFF -DSDL_TEST=OFF \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PWD/sdl2-ios"
  ninja -C SDL/build-ios install
  chmod +x sdl2-ios/bin/sdl2-config
fi

# --- DXVK v2.7.1 + iOS patch ---
if [ ! -d dxvk ]; then
  git clone --depth 1 --branch v2.7.1 \
    --recurse-submodules=include/native/directx --shallow-submodules \
    https://github.com/doitsujin/dxvk
  git -C dxvk submodule update --init --depth 1 include/vulkan include/spirv
  git -C dxvk apply "$HERE/dxvk-v2.7.1-ios.patch"
fi

# --- Meson cross file (SDK path resolved at run time) ---
cat > dxvk/ios-arm64-cross.txt <<EOF
[binaries]
c = 'clang'
cpp = 'clang++'
objc = 'clang'
objcpp = 'clang++'
ar = 'ar'
strip = 'strip'
sdl2-config = '$PWD/sdl2-ios/bin/sdl2-config'

[host_machine]
system = 'darwin'
subsystem = 'ios'
kernel = 'xnu'
cpu_family = 'aarch64'
cpu = 'arm64'
endian = 'little'

[built-in options]
c_args = ['-arch', 'arm64', '-isysroot', '$SDK', '-miphoneos-version-min=$MIN_IOS']
cpp_args = ['-arch', 'arm64', '-isysroot', '$SDK', '-miphoneos-version-min=$MIN_IOS']
objc_args = ['-arch', 'arm64', '-isysroot', '$SDK', '-miphoneos-version-min=$MIN_IOS']
objcpp_args = ['-arch', 'arm64', '-isysroot', '$SDK', '-miphoneos-version-min=$MIN_IOS']
c_link_args = ['-arch', 'arm64', '-isysroot', '$SDK', '-miphoneos-version-min=$MIN_IOS']
cpp_link_args = ['-arch', 'arm64', '-isysroot', '$SDK', '-miphoneos-version-min=$MIN_IOS']
default_library = 'static'
EOF

# --- Configure + build (d3d9 only; dxgi/d3d8/10/11 not needed by the engine) ---
cd dxvk
meson setup build-ios --cross-file ios-arm64-cross.txt \
  -Dbuildtype=release \
  -Denable_dxgi=false -Denable_d3d8=false -Denable_d3d10=false -Denable_d3d11=false \
  -Denable_d3d9=true \
  -Dnative_sdl2=enabled -Dnative_sdl3=disabled -Dnative_glfw=disabled
ninja -C build-ios

ls -la build-ios/src/d3d9/libdxvk_d3d9.a
lipo -info build-ios/src/d3d9/libdxvk_d3d9.a
nm -gU build-ios/src/d3d9/libdxvk_d3d9.a | grep -E "Direct3DCreate9(Ex)?$"
echo "OK: libdxvk_d3d9.a built for arm64-apple-ios"

# Runtime bring-up (NOT covered here, next frontier): link against MoltenVK
# (KhronosGroup/MoltenVK releases ship an ios xcframework) as the Vulkan
# implementation and give DXVK's SDL2 WSI a UIWindow-backed SDL window;
# COD4 fastfiles carry precompiled SM3 bytecode, so no D3DX/HLSL compiler
# is needed at run time (see DEPENDENCY_MAP §8).
