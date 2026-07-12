// Swift <-> engine bridge for the KisakCOD iOS stub.
#pragma once

// EngineSmoke.cpp — calls real engine functions from libkisakcod.a.
const char *kisak_engine_smoke(void);

// D3D9Smoke.mm — one Clear+readback+Present through DXVK→Vulkan→MoltenVK→Metal.
// Pass a CAMetalLayer* the D3D9 swapchain may own (NOT the stub's main layer).
const char *kisak_d3d9_smoke(void *metalLayer);

// Staged engine boot (docs/wip/BootSmoke-wip.cpp + its scaffold): restore both
// to ios/Stub/ and re-declare kisak_boot_smoke() here when the 74-symbol
// closure (ios/Stub/boot_closure.txt) is scaffolded. See docs/NEXT_SESSION.md.
