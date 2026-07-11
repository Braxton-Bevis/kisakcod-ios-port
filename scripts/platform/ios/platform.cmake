# Sanity check the CMakeLists.txt
if (NOT KISAK_PLATFORM STREQUAL "ios")
    message(FATAL_ERROR "KISAK_PLATFORM is incorrect for building ios.")
endif()

# Set the platform override directory (same mechanism as win32:
# src/_platform/ios/<rel_path> shadows src/<rel_path>)
set(PLATFORM_OVERRIDE_DIR "${SRC_DIR}/_platform/ios")

# Clang flags replacing the MSVC set (/MT /O2 /Ot /MP /permissive- ...).
# The decompiled code was only ever built as 32-bit MSVC x86; these two
# semantic flags paper over undefined behavior it demonstrably relies on:
add_compile_options(
    -fno-strict-aliasing   # pervasive type-punning through pointer casts
    -fwrapv                # signed-overflow wrap assumed by decompiled arithmetic
    -fms-extensions        # MSVC-isms throughout the decomp: __declspec, __forceinline, __int32, anonymous structs
    -w                     # first pass: collect hard errors only, warnings later
    -ferror-limit=25
)

add_compile_definitions(KISAK_IOS)
