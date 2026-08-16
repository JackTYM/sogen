#version 450

// Minimal fixed-function passthrough for D3DFVF_XYZRHW|D3DFVF_DIFFUSE: position is already in
// screen-space pixels (pre-transformed by the app), diffuse is a per-vertex color. Not a general
// FVF/render-state synthesizer (that's M4) -- the one hardcoded shape needed for a first triangle.
layout(location = 0) in vec4 inPositionRhw; // x, y (screen pixels), z (depth), rhw (unused)
layout(location = 1) in vec4 inColor;       // already normalized to 0..1 by the vertex format

layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform PushConstants
{
    vec2 viewportSize;
} pc;

void main()
{
    vec2 ndc = (inPositionRhw.xy / pc.viewportSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc, inPositionRhw.z, 1.0);
    fragColor = inColor;
}
