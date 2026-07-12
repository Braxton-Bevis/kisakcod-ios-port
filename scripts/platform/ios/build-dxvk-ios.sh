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
# Usage: build-dxvk-ios.sh <workdir>   (defaults to ./dxvk-ios-work)
set -euxo pipefail

WORK=${1:-dxvk-ios-work}
HERE=$(cd "$(dirname "$0")" && pwd)
SDK=$(xcrun --sdk iphoneos --show-sdk-path)
MIN_IOS=15.0
mkdir -p "$WORK" && cd "$WORK"

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

# --- Configure + build (d3d9 only; native iOS WSI, no SDL) ---
cd dxvk
meson setup build-ios --cross-file ios-arm64-cross.txt \
  -Dbuildtype=release \
  -Denable_dxgi=false -Denable_d3d8=false -Denable_d3d10=false -Denable_d3d11=false \
  -Denable_d3d9=true \
  -Dnative_sdl2=disabled -Dnative_sdl3=disabled -Dnative_glfw=disabled
ninja -C build-ios

ls -la build-ios/src/d3d9/libdxvk_d3d9.a
lipo -info build-ios/src/d3d9/libdxvk_d3d9.a
nm -gU build-ios/src/d3d9/libdxvk_d3d9.a | grep -E "Direct3DCreate9(Ex)?$"
nm build-ios/src/wsi/libwsi.a | grep -m1 "IosWsiDriver" >/dev/null && echo "OK: iOS WSI backend present"
echo "OK: libdxvk_d3d9.a built for arm64-apple-ios (native CAMetalLayer WSI)"

# MoltenVK (the Vulkan implementation): use the static xcframework slice from
# KhronosGroup/MoltenVK releases (MoltenVK-ios.tar -> static/MoltenVK.xcframework/
# ios-arm64/libMoltenVK.a) — remember: force_load + DEAD_CODE_STRIPPING=NO.
