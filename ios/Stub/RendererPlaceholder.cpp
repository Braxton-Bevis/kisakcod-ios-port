// Generated, asset-free renderer proof for the physical iOS device
// (D3D9 → DXVK → Vulkan → MoltenVK → Metal — the M12-proven stack; the
// device GPU has none of the simulator's capability gaps).
//
// This is deliberately not R_Init and not mp_killhouse. It builds a typed,
// source-owned placeholder material and a recognizable first-person
// shoot-house scene, queues RC_DRAW_TRIANGLES through the real engine command
// producer, then executes the real IW3 R/RB tessellation and draw path.
// No retail data, texture, model, string, or shader asset is used.
//
// Device-only: main's simulator lane links no DXVK, so this TU compiles
// empty there and the Swift-visible bridge (D3D9Smoke.mm) reports an honest
// "device-only stage, not run" status instead of faking success.

#include "RendererPlaceholder.h"

#if !TARGET_OS_SIMULATOR

#include <d3d9.h>

#include <gfx_d3d/r_buffers.h>
#include <gfx_d3d/r_dvars.h>
#include <gfx_d3d/r_init.h>
#include <gfx_d3d/r_material.h>
#include <gfx_d3d/r_rendercmds.h>
#include <gfx_d3d/r_state.h>
#include <gfx_d3d/rb_backend.h>
#include <gfx_d3d/rb_pixelcost.h>
#include <gfx_d3d/rb_state.h>
#include <gfx_d3d/rb_stats.h>
#include <qcommon/qcommon.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <climits>

namespace {

constexpr int kBackBufferWidth = 640;
constexpr int kBackBufferHeight = 480;
constexpr int kMaxVertices = 512;
constexpr int kMaxIndices = 1536;
constexpr int kDynamicVertexBytes = 64 * 1024;
constexpr int kDynamicIndexCount = 16 * 1024;
constexpr D3DCOLOR kBackground = D3DCOLOR_XRGB(11, 16, 21);

// The queue/consumer boundary is native arm64, not the 32-bit disk ABI.
// These assertions protect the two offsets this proof passes to D3D9.
static_assert(sizeof(void *) == 8, "renderer proof requires arm64 LP64");
static_assert(sizeof(GfxCmdHeader) == 4, "unexpected render-command header");
static_assert(sizeof(GfxCmdDrawTriangles) == 24,
              "LP64 draw-command header must include the 8-byte material pointer");
static_assert(sizeof(GfxVertex) == 32, "generic backend vertex stride changed");
static_assert(offsetof(GfxVertex, color) == 16,
              "D3DCOLOR declaration no longer matches GfxVertex");
static_assert(kMaxVertices <= INT16_MAX && kMaxIndices <= INT16_MAX,
              "draw command uses signed 16-bit counts");

constexpr const char *kSuccessMarker =
    "IW3 R/RB placeholder scene OK — generated assets, "
    "RC_DRAW_TRIANGLES, readback non-background, Present";

char g_result[768] = "FAIL renderer placeholder: not run";
char g_detail[768] = "IW3 R/RB placeholder detail — not run";
bool g_attempted;
bool g_succeeded;
bool g_detailHasEvidence;

struct SceneData {
    float xyzw[kMaxVertices][4];
    float normal[kMaxVertices][3];
    GfxColor color[kMaxVertices];
    float st[kMaxVertices][2];
    uint16_t indices[kMaxIndices];
    int vertexCount;
    int indexCount;
    bool overflowed;
};

struct ProofMaterialResources {
    MaterialVertexDeclaration vertexDecl;
    MaterialVertexShader vertexShader;
    MaterialPixelShader pixelShader;
    MaterialTechnique technique;
    MaterialTechniqueSet techniqueSet;
    Material material;
    GfxStateBits stateBits;
    IDirect3DVertexDeclaration9 *d3dVertexDecl;
    IDirect3DVertexShader9 *d3dVertexShader;
    IDirect3DPixelShader9 *d3dPixelShader;
};

ProofMaterialResources g_material;
GfxViewParms g_placeholderViewParms;

uint32_t Color(unsigned r, unsigned g, unsigned b)
{
    return D3DCOLOR_ARGB(255, r, g, b);
}

const char *Fail(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    std::vsnprintf(g_result, sizeof(g_result), format, args);
    va_end(args);
    if (!g_detailHasEvidence)
        std::snprintf(g_detail, sizeof(g_detail), "%s", g_result);
    return g_result;
}

uint16_t AddVertex(SceneData &scene, float x, float y, uint32_t packedColor,
                   float s, float t)
{
    if (scene.vertexCount >= kMaxVertices) {
        scene.overflowed = true;
        return 0;
    }

    const int index = scene.vertexCount++;
    scene.xyzw[index][0] = x;
    scene.xyzw[index][1] = y;
    scene.xyzw[index][2] = 0.5f;
    scene.xyzw[index][3] = 1.0f;
    scene.normal[index][0] = 0.0f;
    scene.normal[index][1] = 0.0f;
    scene.normal[index][2] = 1.0f;
    scene.color[index].packed = packedColor;
    scene.st[index][0] = s;
    scene.st[index][1] = t;
    return static_cast<uint16_t>(index);
}

void AddIndex(SceneData &scene, uint16_t index)
{
    if (scene.indexCount >= kMaxIndices) {
        scene.overflowed = true;
        return;
    }
    scene.indices[scene.indexCount++] = index;
}

void AddQuad4(SceneData &scene,
              float x0, float y0, float x1, float y1,
              float x2, float y2, float x3, float y3,
              uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3)
{
    if (scene.vertexCount > kMaxVertices - 4
        || scene.indexCount > kMaxIndices - 6) {
        scene.overflowed = true;
        return;
    }

    const uint16_t base = static_cast<uint16_t>(scene.vertexCount);
    AddVertex(scene, x0, y0, c0, 0.0f, 0.0f);
    AddVertex(scene, x1, y1, c1, 1.0f, 0.0f);
    AddVertex(scene, x2, y2, c2, 1.0f, 1.0f);
    AddVertex(scene, x3, y3, c3, 0.0f, 1.0f);
    AddIndex(scene, base + 0);
    AddIndex(scene, base + 1);
    AddIndex(scene, base + 2);
    AddIndex(scene, base + 0);
    AddIndex(scene, base + 2);
    AddIndex(scene, base + 3);
}

void AddQuad(SceneData &scene,
             float x0, float y0, float x1, float y1,
             float x2, float y2, float x3, float y3,
             uint32_t color)
{
    AddQuad4(scene, x0, y0, x1, y1, x2, y2, x3, y3,
             color, color, color, color);
}

void AddTri(SceneData &scene,
            float x0, float y0, float x1, float y1, float x2, float y2,
            uint32_t color)
{
    if (scene.vertexCount > kMaxVertices - 3
        || scene.indexCount > kMaxIndices - 3) {
        scene.overflowed = true;
        return;
    }

    const uint16_t base = static_cast<uint16_t>(scene.vertexCount);
    AddVertex(scene, x0, y0, color, 0.0f, 0.0f);
    AddVertex(scene, x1, y1, color, 1.0f, 0.0f);
    AddVertex(scene, x2, y2, color, 0.5f, 1.0f);
    AddIndex(scene, base + 0);
    AddIndex(scene, base + 1);
    AddIndex(scene, base + 2);
}

void AddCrate(SceneData &scene, float left, float bottom, float width,
              float height, float skew)
{
    const float right = left + width;
    const float top = bottom + height;
    const uint32_t topColor = Color(151, 119, 72);
    const uint32_t sideColor = Color(82, 62, 39);
    const uint32_t frontColor = Color(118, 88, 51);
    const uint32_t trimColor = Color(54, 43, 29);

    AddQuad(scene, left, top, right, top, right + skew, top + skew,
            left + skew, top + skew, topColor);
    AddQuad(scene, right, bottom, right, top, right + skew, top + skew,
            right + skew, bottom + skew, sideColor);
    AddQuad4(scene, left, bottom, right, bottom, right, top, left, top,
             Color(101, 72, 42), Color(109, 78, 44),
             Color(132, 98, 57), Color(124, 91, 53));

    const float trim = width * 0.07f;
    AddQuad(scene, left, bottom, left + trim, bottom,
            left + trim, top, left, top, trimColor);
    AddQuad(scene, right - trim, bottom, right, bottom,
            right, top, right - trim, top, trimColor);
    AddQuad(scene, left, bottom + height * 0.44f, right, bottom + height * 0.44f,
            right, bottom + height * 0.53f, left, bottom + height * 0.53f,
            trimColor);
    AddQuad(scene, left + width * 0.12f, bottom + height * 0.12f,
            left + width * 0.18f, bottom + height * 0.12f,
            right - width * 0.12f, top - height * 0.12f,
            right - width * 0.18f, top - height * 0.12f, trimColor);
}

void AddTarget(SceneData &scene, float cx, float baseY, float scale,
               uint32_t silhouette)
{
    const float standWidth = 0.018f * scale;
    AddQuad(scene, cx - standWidth, baseY, cx + standWidth, baseY,
            cx + standWidth, baseY + 0.18f * scale,
            cx - standWidth, baseY + 0.18f * scale, Color(71, 58, 41));

    AddQuad(scene, cx - 0.065f * scale, baseY + 0.16f * scale,
            cx + 0.065f * scale, baseY + 0.16f * scale,
            cx + 0.05f * scale, baseY + 0.34f * scale,
            cx - 0.05f * scale, baseY + 0.34f * scale, silhouette);
    AddQuad(scene, cx - 0.105f * scale, baseY + 0.29f * scale,
            cx + 0.105f * scale, baseY + 0.29f * scale,
            cx + 0.08f * scale, baseY + 0.35f * scale,
            cx - 0.08f * scale, baseY + 0.35f * scale, silhouette);

    // Eight source-owned triangles make the round head; no texture/model is
    // smuggled into the proof.
    const float headY = baseY + 0.405f * scale;
    const float rx = 0.052f * scale;
    const float ry = 0.064f * scale;
    const float px[8] = {0.0f, 0.707f, 1.0f, 0.707f,
                         0.0f, -0.707f, -1.0f, -0.707f};
    const float py[8] = {1.0f, 0.707f, 0.0f, -0.707f,
                         -1.0f, -0.707f, 0.0f, 0.707f};
    for (int i = 0; i < 8; ++i) {
        const int next = (i + 1) & 7;
        AddTri(scene, cx, headY,
               cx + px[i] * rx, headY + py[i] * ry,
               cx + px[next] * rx, headY + py[next] * ry,
               silhouette);
    }
}

void BuildShootHouseScene(SceneData &scene)
{
    scene.vertexCount = 0;
    scene.indexCount = 0;
    scene.overflowed = false;

    const uint32_t concreteDark = Color(47, 53, 54);
    const uint32_t concreteMid = Color(78, 83, 80);
    const uint32_t concreteLight = Color(112, 112, 101);
    const uint32_t plywood = Color(133, 103, 62);
    const uint32_t plywoodLight = Color(168, 132, 78);
    const uint32_t steel = Color(45, 54, 58);
    const uint32_t shadow = Color(18, 23, 24);
    const uint32_t warning = Color(218, 168, 39);

    // Perspective shell: far wall, floor, ceiling, and converging side walls.
    AddQuad4(scene, -0.50f, -0.18f, 0.50f, -0.18f,
             0.50f, 0.24f, -0.50f, 0.24f,
             concreteMid, concreteMid, concreteLight, concreteLight);
    AddQuad4(scene, -1.0f, -1.0f, 1.0f, -1.0f,
             0.50f, -0.18f, -0.50f, -0.18f,
             Color(40, 43, 42), Color(48, 49, 45),
             Color(89, 84, 70), Color(76, 73, 64));
    AddQuad4(scene, -1.0f, 1.0f, -0.50f, 0.24f,
             0.50f, 0.24f, 1.0f, 1.0f,
             Color(28, 34, 36), Color(73, 76, 72),
             Color(83, 84, 77), Color(35, 40, 42));
    AddQuad4(scene, -1.0f, -1.0f, -0.50f, -0.18f,
             -0.50f, 0.24f, -1.0f, 1.0f,
             concreteDark, Color(88, 84, 69),
             Color(99, 97, 86), Color(55, 62, 62));
    AddQuad4(scene, 0.50f, -0.18f, 1.0f, -1.0f,
             1.0f, 1.0f, 0.50f, 0.24f,
             Color(75, 71, 59), Color(35, 39, 39),
             Color(49, 56, 57), Color(92, 91, 81));

    // Floor lane seams and a central worn firing lane.
    AddQuad4(scene, -0.045f, -1.0f, 0.045f, -1.0f,
             0.018f, -0.18f, -0.018f, -0.18f,
             Color(92, 77, 51), Color(92, 77, 51),
             Color(125, 104, 66), Color(125, 104, 66));
    AddQuad(scene, -0.64f, -1.0f, -0.60f, -1.0f,
            -0.28f, -0.18f, -0.30f, -0.18f, Color(32, 34, 33));
    AddQuad(scene, 0.60f, -1.0f, 0.64f, -1.0f,
            0.30f, -0.18f, 0.28f, -0.18f, Color(32, 34, 33));
    for (int band = 0; band < 4; ++band) {
        const float y = -0.34f - 0.15f * band;
        const float halfWidth = 0.58f + 0.10f * band;
        AddQuad(scene, -halfWidth, y, halfWidth, y,
                halfWidth + 0.015f, y - 0.018f,
                -halfWidth - 0.015f, y - 0.018f, Color(33, 36, 35));
    }

    // Far doorway, steel surround, and warning bar establish a playable lane.
    AddQuad(scene, -0.14f, -0.18f, 0.14f, -0.18f,
            0.14f, 0.15f, -0.14f, 0.15f, shadow);
    AddQuad(scene, -0.17f, -0.18f, -0.14f, -0.18f,
            -0.14f, 0.18f, -0.17f, 0.18f, steel);
    AddQuad(scene, 0.14f, -0.18f, 0.17f, -0.18f,
            0.17f, 0.18f, 0.14f, 0.18f, steel);
    AddQuad(scene, -0.17f, 0.15f, 0.17f, 0.15f,
            0.17f, 0.19f, -0.17f, 0.19f, steel);
    AddQuad(scene, -0.11f, 0.075f, 0.11f, 0.075f,
            0.11f, 0.105f, -0.11f, 0.105f, warning);

    // Concrete course lines and overhead roof beams.
    AddQuad(scene, -0.49f, 0.005f, -0.17f, 0.005f,
            -0.17f, 0.025f, -0.49f, 0.025f, Color(66, 67, 63));
    AddQuad(scene, 0.17f, 0.005f, 0.49f, 0.005f,
            0.49f, 0.025f, 0.17f, 0.025f, Color(66, 67, 63));
    AddQuad(scene, -1.0f, 0.73f, 1.0f, 0.73f,
            0.82f, 0.62f, -0.82f, 0.62f, steel);
    AddQuad(scene, -0.73f, 1.0f, -0.56f, 1.0f,
            -0.28f, 0.24f, -0.34f, 0.24f, steel);
    AddQuad(scene, 0.56f, 1.0f, 0.73f, 1.0f,
            0.34f, 0.24f, 0.28f, 0.24f, steel);

    // Left plywood shoot-house partition with a real opening (dark inset).
    AddQuad4(scene, -0.93f, -0.66f, -0.46f, -0.27f,
             -0.46f, 0.58f, -0.93f, 0.88f,
             Color(88, 65, 39), plywood, plywoodLight, Color(116, 91, 59));
    AddQuad(scene, -0.82f, -0.10f, -0.51f, -0.015f,
            -0.51f, 0.27f, -0.82f, 0.35f, shadow);
    AddQuad(scene, -0.84f, 0.34f, -0.49f, 0.26f,
            -0.49f, 0.31f, -0.84f, 0.40f, Color(55, 43, 29));
    AddQuad(scene, -0.84f, -0.14f, -0.80f, -0.12f,
            -0.80f, 0.39f, -0.84f, 0.40f, Color(56, 43, 28));
    AddQuad(scene, -0.53f, -0.04f, -0.49f, -0.03f,
            -0.49f, 0.31f, -0.53f, 0.30f, Color(56, 43, 28));

    // Right partition and its offset doorway make the scene asymmetric.
    AddQuad4(scene, 0.43f, -0.26f, 0.94f, -0.72f,
             0.94f, 0.72f, 0.43f, 0.45f,
             plywood, Color(84, 62, 37), Color(103, 78, 48), plywoodLight);
    AddQuad(scene, 0.50f, -0.15f, 0.82f, -0.30f,
            0.82f, 0.31f, 0.50f, 0.28f, shadow);
    AddQuad(scene, 0.47f, 0.27f, 0.85f, 0.30f,
            0.85f, 0.36f, 0.47f, 0.33f, Color(55, 42, 27));
    AddQuad(scene, 0.47f, -0.18f, 0.52f, -0.16f,
            0.52f, 0.31f, 0.47f, 0.33f, Color(55, 42, 27));

    // Source-generated target silhouettes at two different lane depths.
    AddTarget(scene, -0.31f, -0.18f, 0.60f, Color(49, 46, 39));
    AddTarget(scene, 0.29f, -0.25f, 0.86f, Color(61, 55, 43));
    AddQuad(scene, 0.235f, -0.03f, 0.345f, -0.03f,
            0.33f, 0.065f, 0.25f, 0.065f, Color(185, 151, 82));

    // Foreground cover objects carry enough parallax cues to read as boxes,
    // not a collection of disconnected triangles.
    AddCrate(scene, -0.82f, -0.88f, 0.34f, 0.34f, 0.07f);
    AddCrate(scene, 0.50f, -0.82f, 0.30f, 0.30f, 0.06f);
    AddCrate(scene, 0.31f, -0.57f, 0.22f, 0.22f, 0.04f);

    // Small safety placards and hazard stripes are generated geometry too.
    AddQuad(scene, -0.91f, 0.51f, -0.66f, 0.45f,
            -0.66f, 0.58f, -0.91f, 0.65f, Color(203, 190, 135));
    AddQuad(scene, -0.88f, 0.54f, -0.70f, 0.50f,
            -0.70f, 0.535f, -0.88f, 0.58f, Color(161, 53, 35));
    for (int stripe = 0; stripe < 5; ++stripe) {
        const float x = 0.56f + stripe * 0.055f;
        AddQuad(scene, x, -0.36f, x + 0.025f, -0.38f,
                x + 0.075f, -0.31f, x + 0.05f, -0.295f,
                (stripe & 1) ? shadow : warning);
    }

    // A low, abstract weapon silhouette and a compact crosshair establish an
    // unmistakable first-person viewpoint without pretending to be a retail
    // weapon/model asset.
    AddTri(scene, -0.17f, -1.0f, 0.22f, -1.0f, 0.065f, -0.67f,
           Color(24, 28, 29));
    AddQuad(scene, -0.035f, -1.0f, 0.12f, -1.0f,
            0.105f, -0.62f, 0.025f, -0.62f, Color(38, 43, 44));
    AddQuad(scene, 0.035f, -0.66f, 0.095f, -0.66f,
            0.080f, -0.50f, 0.045f, -0.50f, Color(52, 57, 56));
    const uint32_t crosshair = Color(224, 224, 205);
    AddQuad(scene, -0.045f, -0.004f, -0.012f, -0.004f,
            -0.012f, 0.004f, -0.045f, 0.004f, crosshair);
    AddQuad(scene, 0.012f, -0.004f, 0.045f, -0.004f,
            0.045f, 0.004f, 0.012f, 0.004f, crosshair);
    AddQuad(scene, -0.004f, 0.012f, 0.004f, 0.012f,
            0.004f, 0.045f, -0.004f, 0.045f, crosshair);
    AddQuad(scene, -0.004f, -0.045f, 0.004f, -0.045f,
            0.004f, -0.012f, -0.004f, -0.012f, crosshair);
}

// Source used to create these shader-model-3 programs with the Windows SDK's
// d3dcompiler_47 D3DCompile entry point, entry `main`, O3, then with the CTAB
// comment token removed. The executable token streams are committed so CI
// never downloads or runs a shader compiler.
//
// VS HLSL:
//   struct I { float4 p:POSITION0; float4 c:COLOR0; };
//   struct O { float4 p:POSITION0; float4 c:COLOR0; };
//   O main(I i) { O o; o.p=i.p; o.c=i.c; return o; }
// 80 bytes, SHA-256
// 72bffb09db871ba09ec95570ea69b611b742fbd4f79fa53da387c032fbe892af
const DWORD kVertexShader[] = {
    0xFFFE0300u,
    0x0200001Fu, 0x80000000u, 0x900F0000u,
    0x0200001Fu, 0x8000000Au, 0x900F0001u,
    0x0200001Fu, 0x80000000u, 0xE00F0000u,
    0x0200001Fu, 0x8000000Au, 0xE00F0001u,
    0x02000001u, 0xE00F0000u, 0x90E40000u,
    0x02000001u, 0xE00F0001u, 0x90E40001u,
    0x0000FFFFu
};

// PS HLSL: float4 main(float4 c:COLOR0):COLOR0 { return c; }
// 32 bytes, SHA-256
// daf5d3d7c475e2c3d647dc337d48321b9f184df6322e31ef4c0cfc4cec8b0976
const DWORD kPixelShader[] = {
    0xFFFF0300u,
    0x0200001Fu, 0x8000000Au, 0x900F0000u,
    0x02000001u, 0x800F0800u, 0x90E40000u,
    0x0000FFFFu
};

bool EnsureProofDvars()
{
    static const char *rendererNames[] = {
        "Shader Model 2.0", "Shader Model 3.0", "Default", nullptr
    };
    static const char *colorMapNames[] = {
        "Black", "Unchanged", "White", "Gray", nullptr
    };
    static const char *normalMapNames[] = {
        "Flat", "Unchanged", nullptr
    };
    static const char *aaAlphaNames[] = {
        "off", "dither (fast)", "supersample (nice)", nullptr
    };

    // The Stage-B Com_Init boundary intentionally stops before R_RegisterDvars.
    // Register only the exact real dvars dereferenced by this bounded backend
    // path; each uses its production name/default/domain.
    if (!r_logFile) {
        r_logFile = Dvar_RegisterInt("r_logFile", 0, DvarLimits(0, INT_MAX),
                                    DVAR_NOFLAG,
                                    "Write all graphics hardware calls for this many frames to a logfile");
    }
    if (!r_rendererInUse) {
        r_rendererInUse = Dvar_RegisterEnum("r_rendererInUse", rendererNames, 2,
                                            DVAR_ROM,
                                            "The renderer currently used");
    }
    if (!r_drawPrimCap) {
        r_drawPrimCap = Dvar_RegisterInt("r_drawPrimCap", 0,
                                         DvarLimits(-1, 10000), DVAR_CHEAT,
                                         "Only draw primitive batches with less than this many triangles");
    }
    if (!r_drawPrimFloor) {
        r_drawPrimFloor = Dvar_RegisterInt("r_drawPrimFloor", 0,
                                           DvarLimits(0, 10000), DVAR_CHEAT,
                                           "Only draw primitive batches with more than this many triangles");
    }
    if (!r_skipDrawTris) {
        r_skipDrawTris = Dvar_RegisterBool("r_skipDrawTris", false, DVAR_CHEAT,
                                           "Skip drawing primitive tris.");
    }
    if (!r_drawWater) {
        r_drawWater = Dvar_RegisterBool("r_drawWater", true, DVAR_ARCHIVE,
                                        "Enable water animation");
    }
    if (!r_colorMap) {
        r_colorMap = Dvar_RegisterEnum("r_colorMap", colorMapNames, 1,
                                       DVAR_CHEAT,
                                       "Replace all color maps with pure black or pure white");
    }
    if (!r_normalMap) {
        r_normalMap = Dvar_RegisterEnum("r_normalMap", normalMapNames, 1,
                                        DVAR_CHEAT,
                                        "Replace all normal maps with a flat normal map");
    }
    if (!r_specularMap) {
        r_specularMap = Dvar_RegisterEnum(
            "r_specularMap", colorMapNames, 1, DVAR_CHEAT,
            "Replace all specular maps with pure black (off) or pure white (super shiny)");
    }
    if (!r_scaleViewport) {
        r_scaleViewport = Dvar_RegisterFloat(
            "r_scaleViewport", 1.0f, 0.0f, 1.0f, DVAR_CHEAT,
            "Scale 3D viewports by this fraction.  Use this to see if framerate is pixel shader bound.");
    }
    if (!r_aaAlpha) {
        r_aaAlpha = Dvar_RegisterEnum(
            "r_aaAlpha", aaAlphaNames, 1, DVAR_ARCHIVE,
            "Transparency anti-aliasing method");
    }
    if (!r_polygonOffsetScale) {
        r_polygonOffsetScale = Dvar_RegisterFloat(
            "r_polygonOffsetScale", -1.0f, -4.0f, 0.0f, DVAR_ARCHIVE,
            "Offset scale for decal polygons; bigger values z-fight less but poke through walls more");
    }
    if (!r_polygonOffsetBias) {
        r_polygonOffsetBias = Dvar_RegisterFloat(
            "r_polygonOffsetBias", -1.0f, -16.0f, 0.0f, DVAR_ARCHIVE,
            "Offset bias for decal polygons; bigger values z-fight less but poke through walls more");
    }
    if (!sm_polygonOffsetScale) {
        sm_polygonOffsetScale = Dvar_RegisterFloat(
            "sm_polygonOffsetScale", 2.0f, 0.0f, 8.0f, DVAR_NOFLAG,
            "Shadow map offset scale");
    }
    if (!sm_polygonOffsetBias) {
        sm_polygonOffsetBias = Dvar_RegisterFloat(
            "sm_polygonOffsetBias", 0.5f, 0.0f, 32.0f, DVAR_NOFLAG,
            "Shadow map offset bias");
    }

    return r_logFile && r_rendererInUse && r_drawPrimCap
        && r_drawPrimFloor && r_skipDrawTris && r_drawWater && r_colorMap
        && r_normalMap && r_specularMap && r_scaleViewport && r_aaAlpha
        && r_polygonOffsetScale && r_polygonOffsetBias
        && sm_polygonOffsetScale && sm_polygonOffsetBias;
}

HRESULT CreateProofMaterial(IDirect3DDevice9 *device)
{
    HRESULT hr = device->CreateVertexShader(kVertexShader,
                                             &g_material.d3dVertexShader);
    if (FAILED(hr))
        return hr;
    hr = device->CreatePixelShader(kPixelShader, &g_material.d3dPixelShader);
    if (FAILED(hr))
        return hr;

    const D3DVERTEXELEMENT9 elements[] = {
        {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_POSITION, 0},
        {0, 16, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_COLOR, 0},
        D3DDECL_END()
    };
    hr = device->CreateVertexDeclaration(elements, &g_material.d3dVertexDecl);
    if (FAILED(hr))
        return hr;

    g_material.vertexDecl.streamCount = 1;
    g_material.vertexDecl.hasOptionalSource = false;
    g_material.vertexDecl.isLoaded = true;
    g_material.vertexDecl.routing.decl[VERTDECL_GENERIC] =
        g_material.d3dVertexDecl;

    g_material.vertexShader.name = "bmk4_generated_position_color_vs";
    g_material.vertexShader.prog.vs = g_material.d3dVertexShader;
    g_material.vertexShader.prog.loadDef.program =
        const_cast<DWORD *>(kVertexShader);
    // IW3 stores shader programSize in DWORDs, not bytes (the production
    // loader multiplies it by four when validating/uploading the stream).
    g_material.vertexShader.prog.loadDef.programSize =
        static_cast<uint16_t>(sizeof(kVertexShader) / sizeof(kVertexShader[0]));
    g_material.vertexShader.prog.loadDef.loadForRenderer =
        static_cast<uint16_t>(r_rendererInUse->current.integer);

    g_material.pixelShader.name = "bmk4_generated_vertex_color_ps";
    g_material.pixelShader.prog.ps = g_material.d3dPixelShader;
    g_material.pixelShader.prog.loadDef.program =
        const_cast<DWORD *>(kPixelShader);
    g_material.pixelShader.prog.loadDef.programSize =
        static_cast<uint16_t>(sizeof(kPixelShader) / sizeof(kPixelShader[0]));
    g_material.pixelShader.prog.loadDef.loadForRenderer =
        static_cast<uint16_t>(r_rendererInUse->current.integer);

    g_material.technique.name = "bmk4_generated_unlit";
    g_material.technique.passCount = 1;
    g_material.technique.passArray[0].vertexDecl = &g_material.vertexDecl;
    g_material.technique.passArray[0].vertexShader = &g_material.vertexShader;
    g_material.technique.passArray[0].pixelShader = &g_material.pixelShader;

    g_material.techniqueSet.name = "bmk4_generated_techset";
    g_material.techniqueSet.worldVertFormat = VERTDECL_GENERIC;
    g_material.techniqueSet.hasBeenUploaded = true;
    g_material.techniqueSet.remappedTechniqueSet = &g_material.techniqueSet;
    g_material.techniqueSet.techniques[TECHNIQUE_UNLIT] =
        &g_material.technique;

    g_material.stateBits.loadBits[0] = GFXS0_ATEST_DISABLE
        | GFXS0_CULL_NONE | GFXS0_COLORWRITE_RGB | GFXS0_COLORWRITE_ALPHA;
    g_material.stateBits.loadBits[1] = GFXS1_DEPTHTEST_DISABLE;

    g_material.material.info.name = "bmk4_generated_placeholder_material";
    g_material.material.stateBitsEntry[TECHNIQUE_UNLIT] = 0;
    g_material.material.stateBitsCount = 1;
    g_material.material.techniqueSet = &g_material.techniqueSet;
    g_material.material.stateBitsTable = &g_material.stateBits;
    return S_OK;
}

HRESULT CreateDynamicBuffers(IDirect3DDevice9 *device)
{
    std::memset(&gfxBuf.dynamicVertexBufferPool[0], 0,
                sizeof(gfxBuf.dynamicVertexBufferPool[0]));
    std::memset(&gfxBuf.dynamicIndexBufferPool[0], 0,
                sizeof(gfxBuf.dynamicIndexBufferPool[0]));

    HRESULT hr = device->CreateVertexBuffer(
        kDynamicVertexBytes, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0,
        D3DPOOL_DEFAULT, &gfxBuf.dynamicVertexBufferPool[0].buffer, nullptr);
    if (FAILED(hr))
        return hr;
    hr = device->CreateIndexBuffer(
        kDynamicIndexCount * static_cast<int>(sizeof(uint16_t)),
        D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
        D3DPOOL_DEFAULT, &gfxBuf.dynamicIndexBufferPool[0].buffer, nullptr);
    if (FAILED(hr))
        return hr;

    gfxBuf.dynamicVertexBufferPool[0].used = 0;
    gfxBuf.dynamicVertexBufferPool[0].total = kDynamicVertexBytes;
    gfxBuf.dynamicVertexBuffer = &gfxBuf.dynamicVertexBufferPool[0];
    gfxBuf.dynamicIndexBufferPool[0].used = 0;
    gfxBuf.dynamicIndexBufferPool[0].total = kDynamicIndexCount;
    gfxBuf.dynamicIndexBuffer = &gfxBuf.dynamicIndexBufferPool[0];
    return S_OK;
}

bool InitializeBackendState(IDirect3D9 *d3d9, IDirect3DDevice9 *device)
{
    dx.d3d9 = d3d9;
    dx.device = device;
    dx.deviceLost = false;
    dx.inScene = false;
    gfxMetrics.hasTransparencyMsaa = false;
    pixelCostMode = GFX_PIXEL_COST_MODE_OFF;
    g_disableRendering = 0;

    std::memset(&gfxCmdBufSourceState, 0, sizeof(gfxCmdBufSourceState));
    std::memset(&gfxCmdBufState, 0, sizeof(gfxCmdBufState));
    std::memset(&tess, 0, sizeof(tess));
    std::memset(&g_frameStatsCur, 0, sizeof(g_frameStatsCur));
    backupPrimStats = nullptr;
    g_primStats = nullptr;
    g_viewStats = &g_frameStatsCur.viewStats[0];

    gfxCmdBufSourceState.viewParms3D = &g_placeholderViewParms;
    // Positions are already in clip space, so make R_Set3D's real mode guard
    // a no-op instead of manufacturing a fake world/view transform.
    gfxCmdBufSourceState.viewMode = VIEW_MODE_3D;
    gfxCmdBufSourceState.renderTargetWidth = kBackBufferWidth;
    gfxCmdBufSourceState.renderTargetHeight = kBackBufferHeight;
    gfxCmdBufSourceState.sceneViewport.x = 0;
    gfxCmdBufSourceState.sceneViewport.y = 0;
    gfxCmdBufSourceState.sceneViewport.width = kBackBufferWidth;
    gfxCmdBufSourceState.sceneViewport.height = kBackBufferHeight;
    gfxCmdBufSourceState.viewportIsDirty = false;

    gfxCmdBufState.prim.device = device;
    gfxCmdBufState.prim.vertDeclType = VERTDECL_GENERIC;
    gfxCmdBufState.depthRangeType = GFX_DEPTH_RANGE_FULL;
    gfxCmdBufState.depthRangeNear = 0.0f;
    gfxCmdBufState.depthRangeFar = 1.0f;
    gfxCmdBufState.renderTargetId = R_RENDERTARGET_FRAME_BUFFER;
    gfxCmdBufState.origTechType = TECHNIQUE_NONE;
    gfxCmdBufState.viewport.x = 0;
    gfxCmdBufState.viewport.y = 0;
    gfxCmdBufState.viewport.width = kBackBufferWidth;
    gfxCmdBufState.viewport.height = kBackBufferHeight;

    const D3DVIEWPORT9 viewport = {
        0, 0, kBackBufferWidth, kBackBufferHeight, 0.0f, 1.0f
    };
    return SUCCEEDED(device->SetViewport(&viewport));
}

HRESULT ReadBack(IDirect3DDevice9 *device, uint32_t &changedPixels,
                 uint32_t &fnv1a, uint32_t &centerPixel,
                 uint32_t &width, uint32_t &height)
{
    IDirect3DSurface9 *backbuffer = nullptr;
    HRESULT hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO,
                                       &backbuffer);
    if (FAILED(hr) || !backbuffer)
        return FAILED(hr) ? hr : E_FAIL;

    D3DSURFACE_DESC desc = {};
    hr = backbuffer->GetDesc(&desc);
    if (FAILED(hr)) {
        backbuffer->Release();
        return hr;
    }

    IDirect3DSurface9 *systemMemory = nullptr;
    hr = device->CreateOffscreenPlainSurface(desc.Width, desc.Height,
                                              desc.Format, D3DPOOL_SYSTEMMEM,
                                              &systemMemory, nullptr);
    if (SUCCEEDED(hr))
        hr = device->GetRenderTargetData(backbuffer, systemMemory);
    backbuffer->Release();
    if (FAILED(hr) || !systemMemory) {
        if (systemMemory)
            systemMemory->Release();
        return FAILED(hr) ? hr : E_FAIL;
    }

    D3DLOCKED_RECT locked = {};
    hr = systemMemory->LockRect(&locked, nullptr, D3DLOCK_READONLY);
    if (FAILED(hr)) {
        systemMemory->Release();
        return hr;
    }

    changedPixels = 0;
    fnv1a = 2166136261u;
    width = desc.Width;
    height = desc.Height;
    const uint32_t backgroundRgb = kBackground & 0x00FFFFFFu;
    for (uint32_t y = 0; y < desc.Height; ++y) {
        const uint8_t *row = static_cast<const uint8_t *>(locked.pBits)
            + static_cast<std::ptrdiff_t>(y) * locked.Pitch;
        const uint32_t *pixels = reinterpret_cast<const uint32_t *>(row);
        for (uint32_t x = 0; x < desc.Width; ++x) {
            const uint32_t pixel = pixels[x];
            if ((pixel & 0x00FFFFFFu) != backgroundRgb)
                ++changedPixels;
            for (int byteIndex = 0; byteIndex < 4; ++byteIndex) {
                fnv1a ^= row[x * 4 + byteIndex];
                fnv1a *= 16777619u;
            }
        }
    }
    const uint8_t *centerRow = static_cast<const uint8_t *>(locked.pBits)
        + static_cast<std::ptrdiff_t>(desc.Height / 2) * locked.Pitch;
    centerPixel = reinterpret_cast<const uint32_t *>(centerRow)[desc.Width / 2];

    systemMemory->UnlockRect();
    systemMemory->Release();
    return S_OK;
}

} // namespace

const char *kisak_renderer_placeholder_run(IDirect3D9 *d3d9,
                                           IDirect3DDevice9 *device)
{
    if (g_succeeded)
        return kSuccessMarker;
    if (g_attempted)
        return g_result;
    g_attempted = true;

    if (!d3d9 || !device)
        return Fail("FAIL renderer placeholder: retained D3D9/device missing");
    if (!EnsureProofDvars())
        return Fail("FAIL renderer placeholder: required renderer dvars unavailable");
    if (!InitializeBackendState(d3d9, device))
        return Fail("FAIL renderer placeholder: SetViewport failed");

    HRESULT hr = CreateProofMaterial(device);
    if (FAILED(hr)) {
        return Fail("FAIL renderer placeholder: typed material/shader setup "
                    "hr=0x%08X", static_cast<unsigned>(hr));
    }
    hr = CreateDynamicBuffers(device);
    if (FAILED(hr)) {
        return Fail("FAIL renderer placeholder: dynamic buffer setup "
                    "hr=0x%08X", static_cast<unsigned>(hr));
    }

    SceneData scene;
    BuildShootHouseScene(scene);
    if (scene.overflowed || scene.vertexCount <= 32 || scene.indexCount <= 96
        || (scene.indexCount % 3) != 0) {
        return Fail("FAIL renderer placeholder: generated scene invalid "
                    "v=%d i=%d overflow=%d", scene.vertexCount,
                    scene.indexCount, scene.overflowed ? 1 : 0);
    }

    hr = device->BeginScene();
    if (FAILED(hr)) {
        return Fail("FAIL renderer placeholder: BeginScene hr=0x%08X",
                    static_cast<unsigned>(hr));
    }
    dx.inScene = true;

    hr = device->Clear(0, nullptr, D3DCLEAR_TARGET, kBackground, 1.0f, 0);
    if (FAILED(hr)) {
        device->EndScene();
        dx.inScene = false;
        return Fail("FAIL renderer placeholder: Clear hr=0x%08X",
                    static_cast<unsigned>(hr));
    }

    R_iOS_ResetCommandList();
    GfxCmdHeader *header = R_iOS_QueueDrawTrianglesCommand(
        &g_material.material, TECHNIQUE_UNLIT,
        scene.vertexCount, scene.xyzw, scene.normal, scene.color, scene.st,
        scene.indexCount, scene.indices);
    if (!header || header->id != RC_DRAW_TRIANGLES || !header->byteCount) {
        R_iOS_EndCommandList();
        device->EndScene();
        dx.inScene = false;
        return Fail("FAIL renderer placeholder: RC_DRAW_TRIANGLES queue rejected");
    }

    const uint16_t commandBytes = header->byteCount;
    GfxRenderCommandExecState execState = {};
    execState.cmd = header;
    RB_DrawTrianglesCmd(&execState);
    const bool exactAdvance = static_cast<const uint8_t *>(execState.cmd)
        == reinterpret_cast<const uint8_t *>(header) + commandBytes;
    R_iOS_EndCommandList();

    const HRESULT endHr = device->EndScene();
    dx.inScene = false;

    const GfxPrimStats &stats =
        g_frameStatsCur.viewStats[0].primStats[GFX_PRIM_STATS_DEBUG];
    const int expectedTriangles = scene.indexCount / 3;
    const bool statsEarned = stats.primCount == 1
        && stats.triCount == expectedTriangles
        && stats.dynamicIndexCount == scene.indexCount
        && stats.dynamicVertexCount == scene.vertexCount;
    const bool tessDrained = tess.indexCount == 0 && tess.vertexCount == 0
        && !tess.finishedFilling && g_primStats == nullptr;
    const bool uploadsEarned = gfxBuf.dynamicVertexBuffer
        && gfxBuf.dynamicIndexBuffer
        && gfxBuf.dynamicVertexBuffer->used
            == static_cast<uint32_t>(scene.vertexCount * sizeof(GfxVertex))
        && gfxBuf.dynamicIndexBuffer->used == scene.indexCount;

    uint32_t changedPixels = 0;
    uint32_t fnv1a = 0;
    uint32_t centerPixel = 0;
    uint32_t readWidth = 0;
    uint32_t readHeight = 0;
    const HRESULT readHr = ReadBack(device, changedPixels, fnv1a, centerPixel,
                                    readWidth, readHeight);
    const uint32_t minimumChanged =
        static_cast<uint32_t>(kBackBufferWidth * kBackBufferHeight / 2);
    const bool readbackEarned = SUCCEEDED(readHr)
        && readWidth == kBackBufferWidth && readHeight == kBackBufferHeight
        && changedPixels >= minimumChanged && fnv1a != 2166136261u;

    const HRESULT presentHr = device->Present(nullptr, nullptr, nullptr, nullptr);

    // Evaluate every gate BEFORE formatting the detail line: an unearned
    // attempt must never produce a detail line that matches the exact earned
    // shape (Sol round 2 — a failed Present previously left an
    // earned-looking render-detail= beside a failing render=).
    const bool proofEarned = !(FAILED(endHr) || !exactAdvance || !statsEarned
        || !tessDrained || !uploadsEarned || !readbackEarned
        || FAILED(presentHr));

    std::snprintf(
        g_detail, sizeof(g_detail),
        "IW3 R/RB placeholder detail — %svertices=%d indices=%d triangles=%d "
        "cmdBytes=%u stats=%d/%d/%d/%d changedPixels=%u fnv1a=0x%08X "
        "center=0x%08X uploads=%u/%d",
        proofEarned ? "" : "NOT EARNED (see render= failure) ",
        scene.vertexCount, scene.indexCount, expectedTriangles,
        static_cast<unsigned>(commandBytes), stats.primCount, stats.triCount,
        stats.dynamicVertexCount, stats.dynamicIndexCount, changedPixels,
        fnv1a, centerPixel,
        gfxBuf.dynamicVertexBuffer ? gfxBuf.dynamicVertexBuffer->used : 0,
        gfxBuf.dynamicIndexBuffer ? gfxBuf.dynamicIndexBuffer->used : 0);
    g_detailHasEvidence = true;

    if (!proofEarned) {
        return Fail(
            "FAIL renderer placeholder: end=0x%08X advance=%d stats=%d "
            "tess=%d uploads=%d read=0x%08X changed=%u fnv=0x%08X "
            "present=0x%08X",
            static_cast<unsigned>(endHr), exactAdvance ? 1 : 0,
            statsEarned ? 1 : 0, tessDrained ? 1 : 0,
            uploadsEarned ? 1 : 0,
            static_cast<unsigned>(readHr), changedPixels, fnv1a,
            static_cast<unsigned>(presentHr));
    }

    g_succeeded = true;
    return kSuccessMarker;
}

const char *kisak_renderer_placeholder_detail_run(void)
{
    return g_detail;
}

#endif // !TARGET_OS_SIMULATOR
