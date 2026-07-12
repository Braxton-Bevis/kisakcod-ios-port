#include <metal_stdlib>
using namespace metal;

struct VOut {
    float4 position [[position]];
    float4 color;
};

// Hardcoded fullscreen-space triangle — the "hello world" that proves shader
// compilation, pipeline state creation, and drawable presentation end to end.
// offset comes from the game controller's left thumbstick (virtual or physical).
vertex VOut stub_vertex(uint vid [[vertex_id]],
                        constant float2 &offset [[buffer(0)]]) {
    const float2 pos[3] = { float2(0.0, 0.65), float2(-0.65, -0.55), float2(0.65, -0.55) };
    const float4 col[3] = { float4(1.0, 0.35, 0.2, 1.0),
                            float4(0.2, 1.0, 0.45, 1.0),
                            float4(0.3, 0.45, 1.0, 1.0) };
    VOut out;
    out.position = float4(pos[vid] * 0.6 + offset, 0.0, 1.0);
    out.color = col[vid];
    return out;
}

// Shader quality tiers — the stub-scale preview of the engine's r_* shader
// dvars (COD4 exposes "Shader quality" the same three-way). Each tier is a
// genuinely different pipeline state with genuinely different fragment cost,
// so switching them in the settings menu is visible and measurable.

// LOW: flat shade, near-zero fragment ALU.
fragment float4 stub_fragment_low(VOut in [[stage_in]]) {
    return float4(0.55, 0.60, 0.75, 1.0);
}

// MEDIUM (default): interpolated vertex color — the original stub shader.
fragment float4 stub_fragment(VOut in [[stage_in]]) {
    return in.color;
}

// HIGH: animated procedural interference pattern layered on the interpolated
// color — deliberately heavier per-fragment ALU, driven by a time uniform.
fragment float4 stub_fragment_high(VOut in [[stage_in]],
                                   constant float &time [[buffer(0)]]) {
    float2 p = in.position.xy * 0.02;
    float wave = sin(p.x + time * 1.7) * cos(p.y - time * 1.3)
               + 0.5 * sin(p.x * 2.3 - time) * sin(p.y * 1.9 + time * 0.7);
    float3 base = in.color.rgb;
    float3 shimmer = float3(0.5 + 0.5 * sin(time + wave * 3.0),
                            0.5 + 0.5 * sin(time * 0.8 + wave * 4.0 + 2.1),
                            0.5 + 0.5 * sin(time * 1.2 + wave * 5.0 + 4.2));
    return float4(mix(base, shimmer, clamp(0.45 + 0.3 * wave, 0.0, 1.0)), 1.0);
}
