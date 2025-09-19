#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out float outAO;

// G-buffer inputs (declared for future use)
layout(set = 2, binding = 0) uniform sampler2D depthTex;
layout(set = 2, binding = 1) uniform sampler2D normalTex;
layout(set = 2, binding = 2) uniform sampler2D noiseTex;

layout(std140, set = 2, binding = 3) uniform SsaoParams {
    mat4 proj;
    mat4 invProj;
    vec2 noiseScale;
    float radius;
    float bias;
    float power;
    int kernelSize;
    vec4 kernel[64];
} U;

void main()
{
    // Placeholder: neutral AO until SSAO is implemented
    outAO = 1.0;
}

