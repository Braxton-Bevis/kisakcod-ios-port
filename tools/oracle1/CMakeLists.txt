# bmk4-oracle1 — Oracle 1: the instrumented Windows engine-loader oracle.
# Links the REAL database module TUs; see tools/oracle1/DESIGN.md.

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "bmk4-oracle1 requires a 32-bit toolchain: the IW3 "
                        "loader's pointer-token arithmetic is ILP32-only.")
endif()

# DXSDK include dir (engine gfx headers pull d3d9 types; headers only, no
# d3d import library is linked). Mirrors scripts/pre_build.cmake.
if(DEFINED CICD)
    set(ORACLE1_DXSDK_INC_DIR "${DXSDK_DIR}/include")
else()
    set(ORACLE1_DXSDK_INC_DIR "$ENV{DXSDK_DIR}/include")
endif()

set(ORACLE1_ENGINE_SOURCES
    "${SRC_DIR}/database/db_assetnames.cpp"
    "${SRC_DIR}/database/db_auth.cpp"
    "${SRC_DIR}/database/db_file_load.cpp"
    "${SRC_DIR}/database/db_load.cpp"
    "${SRC_DIR}/database/db_memory.cpp"
    "${SRC_DIR}/database/db_registry.cpp"
    "${SRC_DIR}/database/db_stream.cpp"
    "${SRC_DIR}/database/db_stream_load.cpp"
    "${SRC_DIR}/database/db_stringtable_load.cpp"
)

set(ORACLE1_TOOL_SOURCES
    oracle1_main.cpp
    oracle1_scaffold.cpp
    oracle1_trace.cpp
)

add_executable(bmk4-oracle1 ${ORACLE1_ENGINE_SOURCES} ${ORACLE1_TOOL_SOURCES})

target_compile_features(bmk4-oracle1 PRIVATE cxx_std_20)
target_include_directories(bmk4-oracle1 PRIVATE
    "${SRC_DIR}"
    "${DEPS_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${ORACLE1_DXSDK_INC_DIR}"
)

# Engine parity defines (mp target) + the oracle instrumentation define +
# RELEASE_ASSERTS so the engine's own fences stay observable in Release.
# BMK4_ORACLE1 is defined for THIS target only; shipping targets never see it.
target_compile_definitions(bmk4-oracle1 PRIVATE
    KISAK_MP CINEMA USE_SEPARATE_BLIT_TEXTURE
    WIN32 _CONSOLE _MBCS
    BMK4_ORACLE1
    RELEASE_ASSERTS
)

# Same 1.1.4-era zlib the engine embeds (provides inflateInit_ for
# db_auth.cpp); target defined in tools/ff_oracle.
target_link_libraries(bmk4-oracle1 PRIVATE bmk4-ff-oracle-zlib)

if(MSVC)
    # /permissive- for engine-header parity with the shipping targets.
    target_compile_options(bmk4-oracle1 PRIVATE /permissive- /MP)
    set_property(TARGET bmk4-oracle1 PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
