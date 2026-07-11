#include <metal_stdlib>
using namespace metal;

struct VOut {
    float4 position [[position]];
    float4 color;
};

// Hardcoded fullscreen-space triangle — the "hello world" that proves shader
// compilation, pipeline state creation, and drawable presentation end to end.
vertex VOut stub_vertex(uint vid [[vertex_id]]) {
    const float2 pos[3] = { float2(0.0, 0.65), float2(-0.65, -0.55), float2(0.65, -0.55) };
    const float4 col[3] = { float4(1.0, 0.35, 0.2, 1.0),
                            float4(0.2, 1.0, 0.45, 1.0),
                            float4(0.3, 0.45, 1.0, 1.0) };
    VOut out;
    out.position = float4(pos[vid], 0.0, 1.0);
    out.color = col[vid];
    return out;
}

fragment float4 stub_fragment(VOut in [[stage_in]]) {
    return in.color;
}
