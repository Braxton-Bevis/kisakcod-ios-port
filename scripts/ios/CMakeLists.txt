message("----- KisakCOD iOS compile probe -----")

# NOT a shipping target. An OBJECT library of representative translation units,
# one or two per subsystem, compiled (never linked) against the real iOS SDK to
# measure the true porting error surface. Files graduate into this list as the
# port advances; the goal state is the full engine file set.
project(KisakCOD-ios-probe)

include(${PLATFORM_DIR}/${KISAK_PLATFORM}/platform.cmake)

set(IOS_PROBE_SOURCES
    "${SRC_DIR}/ios/sys_ios_paths.mm"        # new iOS platform layer (must compile clean)
    "${SRC_DIR}/bgame/bg_pmove.cpp"          # shared game logic (expected most portable)
    "${SRC_DIR}/game_mp/g_main_mp.cpp"       # server game logic
    "${SRC_DIR}/server_mp/sv_main_mp.cpp"    # server core
    "${SRC_DIR}/qcommon/cmd.cpp"             # command system
    "${SRC_DIR}/qcommon/threads.cpp"         # threading (expected Win32-heavy)
    "${SRC_DIR}/universal/com_files.cpp"     # filesystem layer (Objective 3 target)
    "${SRC_DIR}/universal/win_common.cpp"    # sys utils (Win32 in 'universal'!)
    "${SRC_DIR}/script/scr_vm.cpp"           # GSC script VM
    "${SRC_DIR}/xanim/xmodel.cpp"            # animation/model
    "${SRC_DIR}/physics/phys_ode.cpp"        # ODE physics glue
    "${SRC_DIR}/sound/snd_mss.cpp"           # Miles Sound System coupling
    "${SRC_DIR}/gfx_d3d/r_init.cpp"          # D3D9 renderer init (Objective 4 target)
    "${SRC_DIR}/win32/win_main.cpp"          # Win32 entry point (expected worst case)
)

add_library(${PROJECT_NAME} OBJECT ${IOS_PROBE_SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC ${SRC_DIR} ${DEPS_DIR})
target_compile_definitions(${PROJECT_NAME} PUBLIC KISAK_MP)
