// Swift <-> engine bridge for the KisakCOD iOS stub.
#pragma once

// EngineSmoke.cpp — calls real engine functions from libkisakcod.a.
const char *kisak_engine_smoke(void);

// D3D9Smoke.mm — one Clear+readback+Present through DXVK→Vulkan→MoltenVK→Metal.
// Pass a CAMetalLayer* the D3D9 swapchain may own (NOT the stub's main layer).
const char *kisak_d3d9_smoke(void *metalLayer);

// BootSmoke.cpp initializes and behavior-checks the real memory/dvar/command
// subsystems linked from the engine archive.
const char *kisak_boot_smoke(void);
