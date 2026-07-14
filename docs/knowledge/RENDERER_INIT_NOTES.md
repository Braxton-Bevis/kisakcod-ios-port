# COD4/IW3 Renderer Init & Shader Pipeline Knowledge Pack — BMK4 Stage D

*Sonnet research agent under the coordination seat, 2026-07-13. All repo
reads read-only; file:line cited; web claims carry URLs; unverified items
flagged. Sol: adversarial-review before Stage D work builds on this.*

## 1. R_Init call sequence and the window-handle seam (all r_init.cpp)
R_Init (:2970) → Swap_Init, R_Register, R_InitGlobalStructs (dvars via
R_RegisterDvars :4261), R_InitDrawMethod → R_InitGraphicsApi (:3027):
- re-entry: if dx.device → R_InitSystems (:3034)
- else R_PreCreateWindow (:3205): dx.d3d9 = Direct3DCreate9(0x20) (:3215),
  R_ChooseAdapter, R_StoreDirect3DCaps, R_EnumDisplayModes; then
  R_SetWndParms + R_CreateGameWindow (:3701):
  - R_CreateWindow (:3565): #ifdef KISAK_IOS → wndParms->hwnd =
    (HWND__*)Sys_iOS_GetHostWindow() (:3592); #else CreateWindowExA (:3651)
  - R_InitHardware (:3794) → R_CreateDevice (:3968) →
    R_SetD3DPresentParameters (:4000; d3dpp.hDeviceWindow = hwnd :4017) →
    R_CreateDeviceInternal (:4106) → dx.d3d9->CreateDevice(..., hwnd, ...,
    &dx.device) (:4119); if IsFastFileLoad() → R_LoadGraphicsAssets
    (:3800,:3717 — the boot-zone DB_LoadXAssets, see FF_RUNTIME_NOTES §4);
    then R_CreateForInitOrReset, R_Cinematic_Init, RB_SetInitialState,
    R_InitGamma, R_InitScene → R_InitSystems (:3050): R_InitImages,
    Material_Init, R_InitFonts, R_InitLoadWater, R_InitLightDefs,
    R_ClearFogs, R_InitDebug; rg.registered = 1.

SEAM CONFIRMED: src/ios/r_ios_window.h:15 Sys_iOS_GetHostWindow() is
consumed at exactly ONE site (r_init.cpp:3592); the pointer flows
unmodified into d3dpp.hDeviceWindow, CreateDevice, and
dx.windows[].hwnd (R_FinishAttachingToWindow :3789). Per M12
(PORT_JOURNAL:494-497) that handle is the CAMetalLayer-backed pointer the
patched DXVK iOS WSI (src/wsi/ios, DXVK_WSI_DRIVER) already accepts — D2
is "point dx.d3d9 init at the seam", not new plumbing. Other KISAK_IOS
pairs on this path: R_GetMonitorDimensions (:4071), splash no-ops,
Sys_DirectXFatalError (:364).

## 2. Shaders: retail fastfile path uses ZERO D3DX — CONFIRMED
- Load_GfxVertexShaderLoadDef/PixelShaderLoadDef (db_load.cpp:1822,:1833)
  stream raw DWORD bytecode from the zone; Load_CreateMaterialVertexShader
  (r_material.cpp:302) → dx.device->CreateVertexShader(loadDef->program);
  pixel at :287. Shader-argument bindings arrive baked
  (Load_MaterialPass db_load.cpp:1996 → Load_MaterialShaderArgumentArray
  :1976). No reflection, no compilation.
- Built-in materials: Material_Init → Material_LoadBuiltIn →
  Material_Register (r_material.cpp:824-853) does a FASTFILE lookup and
  Com_Error(ERR_FATAL) if missing; the 50-entry list ("$default", "white",
  …) is r_material.cpp:216-268. ⇒ THE MAPLESS FRAME STILL REQUIRES the
  code/common zones (or a synthetic zone providing those materials) —
  Stage D depends on Stage C6 material/techset/image waves + C7 zones, or
  on a crafted minimal synthetic zone. R_InitFonts needs a font asset too.
- Complete D3DX9 reference audit:
  | Symbol | Site | Class |
  |---|---|---|
  | D3DXInitialize | r_init.cpp:560 | string literal in error table, never called |
  | D3DXCreateBuffer | r_material_load_obj.cpp:1780 | KISAK_NO_FASTFILES-only |
  | D3DXCreateBuffer | :1801 | dev text-compile path, dead with real ffs |
  | D3DXCompileShader | :1977 | same dead path |
  | D3DXGetShaderConstantTable | :3646 | same |
  | D3DXGetShaderInput/OutputSemantics | :3665,:3674 | same |
  Reachability: all live under Material_LoadTechnique (:4235), which reads
  loose techniques/%s.tech ONLY when a techset is not already in the
  fastfile-populated table — dead code on the retail boot path. DXVK
  implements core d3d9 (CreateVertexShader/CreatePixelShader) and no
  d3dx9; that is sufficient. CAVEAT: any build defining KISAK_NO_FASTFILES
  reaches D3DXCreateBuffer (a trivial allocator, cheap to stub) — keep
  Stage D builds off that macro.

## 3. Minimal engine-driven mapless frame (the D4 gate)
Frame loop (src/client/cl_scrn.cpp): SCR_UpdateFrame (:218) →
R_BeginFrame → CL_CGameRendering (no-op when disconnected) →
SCR_DrawScreenField (:123): R_BeginSharedCmdList, R_AddCmdProjectionSet2D,
SCR_ClearScreen path when not in-game, UI_Refresh if key catcher →
R_AddCmdDrawProfile, Con_DrawConsole(0), DevGui_Draw(0) → R_EndFrame
(r_rendercmds.cpp:1453) → R_IssueRenderCommands (:267).
Smallest milestone between triangle and menu: device up (§1) +
R_InitSystems satisfied (built-ins from a loaded/synthetic zone, §2) +
full-screen clear + console 2D pass + Present. No clipmap/gfxworld/xmodel.
(Exact StretchPic/DrawText internals not traced this pass — flagged.)

## 4. DXVK/MoltenVK gaps and knobs
- NULL-DESCRIPTOR GAP (D1, the pre-D2 blocker): MoltenVK 1.4.1 lacks
  VK_EXT_robustness2; DXVK's descriptor path writes VK_NULL_HANDLE for
  unbound slots (dxvk_context.cpp ~:6326-6336 in v2.7.1) — legal only
  under nullDescriptor. Real SM2/SM3 materials leave slots unbound
  routinely. Fix direction (M12 addendum): extend DXVK's dummyResources()
  pattern to descriptor writes + vertex buffers on Apple. Fork-side work;
  NO config key can dodge it.
- No d3d9 FFP/SWVP toggles exist in v2.7.1 dxvk.conf (d3d9.shaderModel is
  capability reporting, not emulation control; swvp*Count keys are
  newer-than-pinned — flagged unverified).
- MVK_CONFIG worth setting at bring-up: MVK_CONFIG_DEBUG=1 (shader
  conversion logging), MVK_CONFIG_RESUME_LOST_DEVICE=1 (defensive),
  confirm MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS stays default-1 (engine has
  its own worker-thread model, r_init.cpp:3816-3822). No MVK keys exist
  for BC-texture or geometry-shader gaps (hard feature facts).
- textureCompressionBC: fine on M-class GPUs (target iPad is M5); A-series
  would need transcode — out of scope.
- Prior art: DXVK issue #4886 declares DXVK-on-iOS impossible — in the
  Wine/x86/JIT framing. Static arm64 link against MoltenVK (this project)
  sidesteps every listed objection, and M12 empirically contradicts the
  blanket claim. No other DXVK-d3d9-on-iOS precedent found: M12 appears to
  be a genuine first (non-exhaustive search — flagged).

URLs: github.com/doitsujin/dxvk/issues/4886 ·
github.com/doitsujin/dxvk/blob/v2.7.1/dxvk.conf ·
github.com/doitsujin/dxvk/blob/v2.7.1/src/dxvk/dxvk_context.cpp ·
github.com/KhronosGroup/MoltenVK/blob/main/Docs/MoltenVK_Configuration_Parameters.md

## Flagged
1. 2D draw-command internals (StretchPic/DrawText sites) untraced.
2. swvp dxvk.conf keys unverified for the pinned fork.
3. "First DXVK d3d9 frame on iOS" claim from non-exhaustive search.

## ADDENDUM (2026-07-14): prior-art fact-check outcome

A dedicated adversarial search REFUTED the broad "first D3D9 frame through
DXVK on iOS" claim: Ammaar Reshi's Command & Conquer: Generals — Zero Hour
native iOS port (github.com/ammaarreshi/Generals-Mac-iOS-iPad, announced
2026-07-04, one week before M12) rendered D3D8 gameplay on iPhone/iPad
through DXVK — and DXVK's D3D8 frontend delegates to its D3D9
implementation (d3d8_device.cpp holds an IDirect3DDevice9; Present
delegates), so frames had traversed DXVK's D3D9-backed code on iOS first.
What survives: BMK4/M12 is the first publicly documented application
calling DXVK's D3D9 FRONTEND directly on a physical iOS/iPadOS device,
as a smoke-test frame (Clear/readback/Present), not a game scene.
README and repo description were corrected accordingly (commit 70498f1).
Search coverage and links are in the fact-check transcript; UTM's Neptune
D3D11/Venus work is a macOS-path near-miss worth watching.
