#version 450

layout(location = 0) in vec2 fragUV;

layout(set = 0, binding = 0) uniform SSAOKernel {
    vec4 samples[64];
} uKernel;
layout(set = 0, binding = 1) uniform sampler2D noiseTex;
layout(set = 0, binding = 2) uniform sampler2D normalTex;
layout(set = 0, binding = 3) uniform sampler2D depthTex;

layout(location = 0) out float outOcclusion;

void main() {
    vec3 normal = normalize(texture(normalTex, fragUV).xyz * 2.0 - 1.0);
    float baseOcclusion = clamp(1.0 - normal.z, 0.0, 1.0);
    float noiseContribution = texture(noiseTex, fragUV * 4.0).x * 0.1;
    outOcclusion = clamp(baseOcclusion + noiseContribution, 0.0, 1.0);
}
