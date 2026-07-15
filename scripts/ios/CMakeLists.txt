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
    "${SRC_DIR}/ios/sys_ios_events.cpp"       # B4 real Sys_QueEvent/Sys_GetEvent ring
    "${SRC_DIR}/ios/msvc_crt_compat.cpp"     # MSVC CRT functions with no libc equivalent
    "${SRC_DIR}/ios/snd_ios_stub.cpp"        # no-op Miles (AIL_*) backend, pre-AVAudioEngine
    "${SRC_DIR}/bgame/bg_pmove.cpp"          # real player movement entry point
    "${SRC_DIR}/bgame/bg_jump.cpp"           # jump mechanics used by the pmove sandbox
    "${SRC_DIR}/bgame/bg_slidemove.cpp"      # collision slide/step solver used by pmove
    "${SRC_DIR}/bgame/bg_mantle.cpp"         # mantle checks in the real pmove path
    "${SRC_DIR}/game_mp/g_main_mp.cpp"       # server game logic
    "${SRC_DIR}/game_mp/g_utils_mp.cpp"      # server game utilities
    "${SRC_DIR}/server_mp/sv_main_mp.cpp"    # server core
    "${SRC_DIR}/qcommon/cmd.cpp"             # command system
    "${SRC_DIR}/qcommon/com_fileaccess.cpp"  # stdio wrapper used by real filesystem I/O
    "${SRC_DIR}/qcommon/threads.cpp"         # threading (pthreads/mach under KISAK_IOS)
    "${SRC_DIR}/qcommon/huffman.cpp"         # network compression
    "${SRC_DIR}/qcommon/msg_mp.cpp"          # network message (de)serialization
    "${SRC_DIR}/qcommon/net_chan_mp.cpp"     # B3 netchan + in-memory loopback owner
    "${SRC_DIR}/qcommon/unzip.cpp"           # IWD reader required by com_files object closure
    "${SRC_DIR}/script/scr_memorytree.cpp"   # B5 real script-string backing allocator
    "${SRC_DIR}/script/scr_stringlist.cpp"   # B5 real SL_* owners required by M15
    "${SRC_DIR}/stringed/stringed_hooks.cpp" # language dvars/filesystem integration
    "${SRC_DIR}/stringed/stringed_ingame.cpp"# asset-free localization package startup
    "${SRC_DIR}/universal/com_files.cpp"     # filesystem layer (Objective 3 target)
    "${SRC_DIR}/universal/com_shared.cpp"    # Com_FilterPath etc. used by FS file listing
    "${SRC_DIR}/universal/win_common.cpp"    # sys utils (POSIX under KISAK_IOS)
    "${SRC_DIR}/universal/q_shared.cpp"      # shared string/parse utilities
    "${SRC_DIR}/universal/com_math.cpp"      # math library
    "${SRC_DIR}/universal/com_math_anglevectors.cpp" # real per-frame pmove basis vectors
    "${SRC_DIR}/universal/dvar.cpp"          # dvar system (Com_Init dependency)
    "${SRC_DIR}/universal/dvar_cmds.cpp"     # B4 real set/seta command owners
    "${SRC_DIR}/universal/com_memory.cpp"    # zone/hunk memory (mmap under KISAK_IOS)
    "${SRC_DIR}/qcommon/common.cpp"          # Com_Init / Com_Frame — the boot path itself
    "${SRC_DIR}/buildnumber.cpp"             # getBuildNumber — reached by Com_Init version dvar
    "${SRC_DIR}/script/scr_vm.cpp"           # GSC script VM
    "${SRC_DIR}/xanim/xmodel.cpp"            # animation/model
    "${SRC_DIR}/physics/phys_ode.cpp"        # ODE physics glue
    "${SRC_DIR}/sound/snd_mss.cpp"           # Miles Sound System coupling (typed stub on iOS)
    "${SRC_DIR}/win32/win_net.cpp"           # networking (BSD sockets under KISAK_IOS)
    "${SRC_DIR}/win32/win_steam.cpp"         # Steam auth (explicit no-op on iOS)
    "${SRC_DIR}/gfx_d3d/r_init.cpp"          # D3D9 renderer init (Objective 4 target)
    "${SRC_DIR}/gfx_d3d/r_rendercmds.cpp"    # real renderer command producer
    "${SRC_DIR}/gfx_d3d/rb_backend.cpp"      # real renderer command consumer
    "${SRC_DIR}/gfx_d3d/rb_shade.cpp"        # tessellation/material draw path
    "${SRC_DIR}/gfx_d3d/rb_state.cpp"        # backend command-buffer globals/state
    "${SRC_DIR}/gfx_d3d/rb_stats.cpp"        # backend primitive accounting
    "${SRC_DIR}/gfx_d3d/r_state_utils.cpp"   # renderer state transitions
    "${SRC_DIR}/gfx_d3d/r_shade.cpp"         # material pass setup
    "${SRC_DIR}/gfx_d3d/r_state.cpp"         # D3D9 render-state owner
    "${SRC_DIR}/gfx_d3d/r_buffers.cpp"       # dynamic vertex/index buffers
    "${SRC_DIR}/gfx_d3d/r_draw_bsp.cpp"      # real R_SetStreamSource owner
)

add_library(${PROJECT_NAME} OBJECT ${IOS_PROBE_SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC ${SRC_DIR} ${DEPS_DIR})
target_compile_definitions(${PROJECT_NAME} PUBLIC KISAK_MP)
