// Swift <-> engine bridge for the KisakCOD iOS stub.
#pragma once

// EngineSmoke.cpp — calls real engine functions from libkisakcod.a.
const char *kisak_engine_smoke(void);

// D3D9Smoke.mm — one Clear+readback+Present through DXVK→Vulkan→MoltenVK→Metal.
// Pass a CAMetalLayer* the D3D9 swapchain may own (NOT the stub's main layer).
const char *kisak_d3d9_smoke(void *metalLayer);

// BootComInit.cpp is the fresh cold-start path; BootSmoke.cpp re-earns the
// existing M13 behavioral marker without repeating any initializer.
const char *kisak_boot_cominit_stage(void);
const char *kisak_boot_common_spine_status(void);
const char *kisak_boot_probe_after_init(void);

// BootNetSmoke.cpp sends and decodes one real msg through NET's loopback queue.
const char *kisak_boot_net_smoke(void);

// BootEventSmoke.cpp proves the real queue/event/command/dvar path.
const char *kisak_boot_event_smoke(void);

// BootFSSmoke.cpp runs real FS_InitFilesystem and a sandbox file round trip.
const char *kisak_fs_smoke(void);

// BootM15Closeout.cpp verifies all four native closeout behaviors.
const char *kisak_boot_m15_closeout(void);

// BootFFSmoke.cpp proves the slice-7 fastfile kernel (K0 container spine +
// K1 RawFile wire walk) against oracle-qualified synthetic fixture 01.
const char *kisak_ff_kernel_smoke(const char *validPath,
                                  const char *malformedPath);

// PmoveSandbox.cpp drives the real bg_pmove closure in a synthetic z=0 world.
void kisak_pmove_init(void);
const char *kisak_pmove_proof(void);
const char *kisak_pmove_frame(float forwardmove, float rightmove,
                              int jump, int sprint, float dtMs);
