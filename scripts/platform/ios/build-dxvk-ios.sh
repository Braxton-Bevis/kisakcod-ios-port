#!/bin/bash
# Build DXVK's d3d9 module as a static library for arm64-apple-ios.
#
# FRONTIER RESOLVED twice over (PORT_JOURNAL M9 + M12): with the patch next to
# this script, dxvk v2.7.1 builds for iOS AND presents through a native
# CAMetalLayer WSI backend ("iOS" driver) — verified on an iPad Pro (M5):
# CreateDevice D3D_OK, Clear readback pixel-exact, Present D3D_OK.
#
# The patch contains:
#   - Apple compile fixes (__APPLE__ in util_win32_compat.h, lzcnt cast,
#     pthread_setname_np, <cstddef>, _NSGetExecutablePath in getExePath)
#   - static-archive linking on Darwin (no GNU version scripts)
#   - src/wsi/ios: CAMetalLayer WSI backend — the HWND *is* a CAMetalLayer*;
#     surfaces via VK_EXT_metal_surface; select with DXVK_WSI_DRIVER=iOS
#   - vulkan_loader: dlsym(RTLD_DEFAULT) first → statically linked MoltenVK
#   - Apple feature relaxations MoltenVK cannot provide (geometryShader,
#     shaderCullDistance, robustness2 pair, VK_KHR_pipeline_library)
#
# App-side integration requirements (see ios/project.yml + ios/Stub/D3D9Smoke.mm):
#   - link the produced .a set + MoltenVK.xcframework's static libMoltenVK.a
#   - MoltenVK must be -force_load'ed AND DEAD_CODE_STRIPPING=NO (it is only
#     reached via dlsym; the linker would otherwise drop/strip it)
#   - setenv("DXVK_WSI_DRIVER", "iOS") before Direct3DCreate9
#
# Prerequisites (all obtainable without root):
#   - Xcode + iOS SDK            (xcode-select -p must resolve)
#   - meson + ninja              (python3 -m pip install --user meson ninja)
#   - glslang                    (KhronosGroup/glslang releases, osx binary, on PATH)
#
# Usage: build-dxvk-ios.sh <workdir> [iphoneos|iphonesimulator]
#        (defaults to ./dxvk-ios-work and iphoneos)
set -euxo pipefail

WORK=${1:-dxvk-ios-work}
PLATFORM=${2:-iphoneos}
HERE=$(cd "$(dirname "$0")" && pwd)
MIN_IOS=15.0

case "$PLATFORM" in
  iphoneos)
    SDK_NAME=iphoneos
    CROSS_FILE=bmk4-ios-arm64-cross.txt
    BUILD_DIR=build-ios
    COMPILER_ARGS="['-arch', 'arm64', '-isysroot', 'SDK_PATH', '-miphoneos-version-min=$MIN_IOS']"
    RESULT_PLATFORM=arm64-apple-ios
    ;;
  iphonesimulator)
    SDK_NAME=iphonesimulator
    CROSS_FILE=bmk4-ios-simulator-arm64-cross.txt
    BUILD_DIR=build-ios-simulator
    COMPILER_ARGS="['-target', 'arm64-apple-ios${MIN_IOS}-simulator', '-isysroot', 'SDK_PATH']"
    RESULT_PLATFORM=arm64-apple-ios-simulator
    ;;
  *)
    echo "usage: $0 <workdir> [iphoneos|iphonesimulator]" >&2
    exit 2
    ;;
esac

SDK=$(xcrun --sdk "$SDK_NAME" --show-sdk-path)
COMPILER_ARGS=${COMPILER_ARGS//SDK_PATH/$SDK}
mkdir -p "$WORK" && cd "$WORK"

# --- DXVK v2.7.1 + iOS patches ---
if [ ! -d dxvk ]; then
  git clone --depth 1 --branch v2.7.1 \
    --recurse-submodules=include/native/directx --shallow-submodules \
    https://github.com/doitsujin/dxvk
  git -C dxvk submodule update --init --depth 1 include/vulkan include/spirv
fi

apply_or_verify_patch() {
  local patch=$1 label=$2
  if git -C dxvk apply --check "$patch"; then
    git -C dxvk apply "$patch"
  elif git -C dxvk apply --reverse --check "$patch"; then
    echo "OK: $label already applied"
  else
    echo "ERROR: $label is neither cleanly applicable nor already applied" >&2
    exit 1
  fi
}

# D1 must follow the base iOS port. The reverse checks make a reused workdir
# safe while refusing an unknown or partially patched DXVK source tree.
apply_or_verify_patch "$HERE/dxvk-v2.7.1-ios.patch" "DXVK iOS base patch"
apply_or_verify_patch "$HERE/dxvk-v2.7.1-ios-null-descriptor.patch" "DXVK D1 null-descriptor patch"

# --- Meson cross file (SDK path resolved at run time) ---
cat > "dxvk/$CROSS_FILE" <<EOF
[binaries]
c = 'clang'
cpp = 'clang++'
objc = 'clang'
objcpp = 'clang++'
ar = 'ar'
strip = 'strip'

[host_machine]
system = 'darwin'
subsystem = 'ios'
kernel = 'xnu'
cpu_family = 'aarch64'
cpu = 'arm64'
endian = 'little'

[built-in options]
c_args = $COMPILER_ARGS
cpp_args = $COMPILER_ARGS
objc_args = $COMPILER_ARGS
objcpp_args = $COMPILER_ARGS
c_link_args = $COMPILER_ARGS
cpp_link_args = $COMPILER_ARGS
default_library = 'static'
EOF

# --- Configure + build (d3d9 only; native iOS WSI, no SDL) ---
cd dxvk
MESON_SETUP_ARGS=(
  --cross-file "$CROSS_FILE"
  -Dbuildtype=release
  -Denable_dxgi=false -Denable_d3d8=false -Denable_d3d10=false -Denable_d3d11=false
  -Denable_d3d9=true
  -Dnative_sdl2=disabled -Dnative_sdl3=disabled -Dnative_glfw=disabled
)
if [ -d "$BUILD_DIR" ]; then
  # A previous invocation may have selected the other Apple SDK. Meson caches
  # cross-machine properties, so reconfigure is insufficient here.
  meson setup --wipe "$BUILD_DIR" "${MESON_SETUP_ARGS[@]}"
else
  meson setup "$BUILD_DIR" "${MESON_SETUP_ARGS[@]}"
fi
ninja -C "$BUILD_DIR"

ls -la "$BUILD_DIR/src/d3d9/libdxvk_d3d9.a"
lipo -info "$BUILD_DIR/src/d3d9/libdxvk_d3d9.a"
nm -gU "$BUILD_DIR/src/d3d9/libdxvk_d3d9.a" | grep -E "Direct3DCreate9(Ex)?$"
nm "$BUILD_DIR/src/wsi/libwsi.a" | grep -m1 "IosWsiDriver" >/dev/null && echo "OK: iOS WSI backend present"
echo "OK: libdxvk_d3d9.a built for $RESULT_PLATFORM (native CAMetalLayer WSI)"

# MoltenVK (the Vulkan implementation): use the static xcframework slice from
# KhronosGroup/MoltenVK releases (MoltenVK-ios.tar -> static/MoltenVK.xcframework/
# ios-arm64/libMoltenVK.a) — remember: force_load + DEAD_CODE_STRIPPING=NO.
