#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out float outAO;

layout(set = 3, binding = 0) uniform sampler2D aoTex;

layout(std140, set = 3, binding = 1) uniform BlurParams {
    vec2 direction; // (1,0) or (0,1)
    float radius;   // sample radius in texels
    float sigma;    // gaussian sigma
} B;

void main()
{
    // Placeholder: pass-through
    outAO = texture(aoTex, vUV).r;
}

