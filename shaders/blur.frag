#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out float outAO;

layout(set = 0, binding = 0) uniform sampler2D aoTex;

layout(std140, set = 0, binding = 1) uniform BlurParams {
    vec2 dir;    // (1,0) or (0,1)
    float radius; // 1..5
    float sigma;  // 0.5..3.0
} U;

// Minimal stub: pass-through the input aoTex value (no blur yet)
void main()
{
    outAO = texture(aoTex, vUV).r;
}
