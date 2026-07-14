cmake_minimum_required(VERSION 3.20)

project(bmk4_layout_conformance LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "The KISAK layout authority is the Win32 ABI")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Configure with -A Win32; pointer size must be four")
endif()
set(KISAK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "BMK4 repository root")
set(DXSDK_INCLUDE "" CACHE PATH "DirectX SDK include directory")
if(NOT EXISTS "${KISAK_ROOT}/src/xanim/xanim.h")
    message(FATAL_ERROR "KISAK_ROOT does not point at the BMK4 repository")
endif()
if(NOT EXISTS "${DXSDK_INCLUDE}")
    message(FATAL_ERROR "DXSDK_INCLUDE is required for the real engine headers")
endif()

add_executable(bmk4-layout-conformance main.cpp)
target_compile_features(bmk4-layout-conformance PRIVATE cxx_std_20)
target_compile_definitions(bmk4-layout-conformance PRIVATE
    KISAK_MP
    WIN32
    _CONSOLE
    _MBCS
    CINEMA
    USE_SEPARATE_BLIT_TEXTURE
)
target_include_directories(bmk4-layout-conformance PRIVATE
    "${KISAK_ROOT}/src"
    "${KISAK_ROOT}/deps"
    "${DXSDK_INCLUDE}"
)
if(MSVC)
    target_compile_options(bmk4-layout-conformance PRIVATE /W3 /permissive-)
endif()
