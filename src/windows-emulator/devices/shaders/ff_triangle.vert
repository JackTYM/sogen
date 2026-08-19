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
    // execute_draw binds a negative-height VkViewport so that clip-space y = +1 lands on the
    // framebuffer's TOP row, matching D3D9's convention (vkd3d-shader translates a D3D9 vertex shader's
    // oPos straight to gl_Position, in D3D9 clip space, with no y compensation of its own). This
    // fixed-function path's input is already in D3D9 SCREEN space -- y = 0 is the top row -- so it must
    // target that same convention: y = 0 has to become clip y = +1, hence the negation.
    vec2 ndc = (inPositionRhw.xy / pc.viewportSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, inPositionRhw.z, 1.0);
    fragColor = inColor;
}
