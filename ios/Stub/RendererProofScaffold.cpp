// Simulator-only link closure for the bounded IW3 R/RB placeholder proof.
//
// The renderer archive intentionally contains only the first 11 real owner
// translation units.  Those members retain relocations to later renderer and
// database waves even though the generated, asset-free proof keeps the
// corresponding modes disabled.  Data below is zero/off by default.  Every
// deferred function aborts with its symbol name if the proof ever reaches it;
// none of these stand-ins may be used to earn the renderer success marker.

#include <TargetConditionals.h>

#if TARGET_OS_SIMULATOR

#include <gfx_d3d/r_dvars.h>
#include <gfx_d3d/r_init.h>
#include <gfx_d3d/r_water.h>
#include <gfx_d3d/rb_logfile.h>
#include <gfx_d3d/rb_pixelcost.h>

#include <cstdio>
#include <cstdlib>

namespace {

[[noreturn]] void RendererProofDeferredOwnerReached(const char *symbol)
{
    std::fprintf(
        stderr,
        "BMK4 renderer proof reached deferred real owner: %s\n",
        symbol);
    std::fflush(stderr);
    std::abort();
}

} // namespace

// Real owner: src/database/db_registry.cpp.  Delete when that TU graduates
// into the simulator renderer archive.
r_globals_t rg{};

// Real owner: src/gfx_d3d/rb_pixelcost.cpp.  The proof does not collect pixel
// cost, so the behavior-matching initial state is OFF.  Delete with that TU.
GfxPixelCostMode pixelCostMode = GFX_PIXEL_COST_MODE_OFF;

// Real owner: src/gfx_d3d/r_dvars.cpp.  RendererPlaceholder.cpp assigns each
// pointer through the real dvar system before entering R/RB, using the
// production name/default/domain.  Delete this entire group when r_dvars.cpp
// graduates into the simulator archive.
const dvar_t *r_logFile = nullptr;
const dvar_t *r_rendererInUse = nullptr;
const dvar_t *r_drawPrimCap = nullptr;
const dvar_t *r_drawPrimFloor = nullptr;
const dvar_t *r_skipDrawTris = nullptr;
const dvar_t *r_drawWater = nullptr;
const dvar_t *r_colorMap = nullptr;
const dvar_t *r_normalMap = nullptr;
const dvar_t *r_specularMap = nullptr;
const dvar_t *r_scaleViewport = nullptr;
const dvar_t *r_aaAlpha = nullptr;
const dvar_t *r_polygonOffsetBias = nullptr;
const dvar_t *r_polygonOffsetScale = nullptr;
const dvar_t *sm_polygonOffsetBias = nullptr;
const dvar_t *sm_polygonOffsetScale = nullptr;

// Real owner for the logging functions below:
// src/gfx_d3d/rb_logfile.cpp.  Logging is disabled by r_logFile; reaching any
// function is a proof-boundary violation.  Delete when that TU graduates.
void __cdecl RB_LogPrint(const char *)
{
    RendererProofDeferredOwnerReached("RB_LogPrint");
}

const char *__cdecl RB_LogTechniqueType(MaterialTechniqueType)
{
    RendererProofDeferredOwnerReached("RB_LogTechniqueType");
}

void __cdecl RB_LogPrintState_0(int, int)
{
    RendererProofDeferredOwnerReached("RB_LogPrintState_0");
}

void __cdecl RB_LogPrintState_1(int, int)
{
    RendererProofDeferredOwnerReached("RB_LogPrintState_1");
}

// Real owner for the pixel-cost functions below:
// src/gfx_d3d/rb_pixelcost.cpp.  pixelCostMode is OFF, so these are unreachable
// on the proof path.  Delete when that TU graduates.
const Material *__cdecl R_PixelCost_GetAccumulationMaterial(const Material *)
{
    RendererProofDeferredOwnerReached("R_PixelCost_GetAccumulationMaterial");
}

void __cdecl R_PixelCost_BeginSurface(GfxCmdBufContext)
{
    RendererProofDeferredOwnerReached("R_PixelCost_BeginSurface");
}

void __cdecl R_PixelCost_EndSurface(GfxCmdBufContext)
{
    RendererProofDeferredOwnerReached("R_PixelCost_EndSurface");
}

// Real owner: src/gfx_d3d/r_water.cpp.  The generated proof material has no
// water arguments.  Delete when r_water.cpp graduates into the archive.
void __cdecl R_UploadWaterTexture(water_t *, float)
{
    RendererProofDeferredOwnerReached("R_UploadWaterTexture");
}

#endif // TARGET_OS_SIMULATOR
