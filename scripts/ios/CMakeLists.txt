message("----- KisakCOD iOS compile probe -----")

# NOT a shipping target. An OBJECT library of representative translation units,
# one or two per subsystem, compiled (never linked) against the real iOS SDK to
# measure the true porting error surface. Files graduate into this list as the
# port advances; the goal state is the full engine file set.
project(KisakCOD-ios-probe)

include(${PLATFORM_DIR}/${KISAK_PLATFORM}/platform.cmake)

set(IOS_PROBE_SOURCES
    "${SRC_DIR}/ios/sys_ios_paths.mm"        # new iOS platform layer (must compile clean)
    "${SRC_DIR}/ios/sys_ios_main.mm"         # iOS platform entry layer (replaces win32/win_main.cpp)
    "${SRC_DIR}/ios/msvc_crt_compat.cpp"     # MSVC CRT functions with no libc equivalent
    "${SRC_DIR}/ios/snd_ios_stub.cpp"        # no-op Miles (AIL_*) backend, pre-AVAudioEngine
    "${SRC_DIR}/bgame/bg_pmove.cpp"          # shared game logic (expected most portable)
    "${SRC_DIR}/game_mp/g_main_mp.cpp"       # server game logic
    "${SRC_DIR}/game_mp/g_utils_mp.cpp"      # server game utilities
    "${SRC_DIR}/server_mp/sv_main_mp.cpp"    # server core
    "${SRC_DIR}/qcommon/cmd.cpp"             # command system
    "${SRC_DIR}/qcommon/threads.cpp"         # threading (pthreads/mach under KISAK_IOS)
    "${SRC_DIR}/qcommon/huffman.cpp"         # network compression
    "${SRC_DIR}/qcommon/msg_mp.cpp"          # network message (de)serialization
    "${SRC_DIR}/universal/com_files.cpp"     # filesystem layer (Objective 3 target)
    "${SRC_DIR}/universal/win_common.cpp"    # sys utils (POSIX under KISAK_IOS)
    "${SRC_DIR}/universal/q_shared.cpp"      # shared string/parse utilities
    "${SRC_DIR}/universal/com_math.cpp"      # math library
    "${SRC_DIR}/script/scr_vm.cpp"           # GSC script VM
    "${SRC_DIR}/xanim/xmodel.cpp"            # animation/model
    "${SRC_DIR}/physics/phys_ode.cpp"        # ODE physics glue
    "${SRC_DIR}/sound/snd_mss.cpp"           # Miles Sound System coupling (typed stub on iOS)
    "${SRC_DIR}/win32/win_net.cpp"           # networking (BSD sockets under KISAK_IOS)
    "${SRC_DIR}/win32/win_steam.cpp"         # Steam auth (explicit no-op on iOS)
    "${SRC_DIR}/gfx_d3d/r_init.cpp"          # D3D9 renderer init (Objective 4 target)
)

add_library(${PROJECT_NAME} OBJECT ${IOS_PROBE_SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC ${SRC_DIR} ${DEPS_DIR})
target_compile_definitions(${PROJECT_NAME} PUBLIC KISAK_MP)
