#version 450

layout(location = 0) in vec2 fragUV;

layout(set = 0, binding = 0) uniform SSAOKernel {
    vec4 samples[64];
} uKernel;
layout(set = 0, binding = 1) uniform sampler2D noiseTex;
layout(set = 0, binding = 2) uniform sampler2D normalTex;
layout(set = 0, binding = 3) uniform sampler2D depthTex;
layout(set = 0, binding = 4) uniform SSAOParams {
    mat4 inverseProjection;
} uParams;

layout(location = 0) out float outOcclusion;

vec3 reconstruct_view_position(float linearDepth, vec2 uv) {
    if (linearDepth <= 0.0) {
        return vec3(0.0);
    }
    vec2 ndc = uv * 2.0 - 1.0;
    vec4 clip = vec4(ndc, 1.0, 1.0);
    vec4 view = uParams.inverseProjection * clip;
    vec3 viewDir = view.xyz / view.w;
    float denom = viewDir.z;
    if (abs(denom) < 1e-4) {
        denom = denom < 0.0 ? -1e-4 : 1e-4;
    }
    float scale = -linearDepth / denom;
    return viewDir * scale;
}

void main() {
    vec2 uv = fragUV;
    vec3 normal = normalize(texture(normalTex, uv).xyz * 2.0 - 1.0);
    float linearDepth = texture(depthTex, uv).r;
    vec3 viewPosition = reconstruct_view_position(linearDepth, uv);

    float baseOcclusion = clamp(1.0 - normal.z, 0.0, 1.0);
    float noiseContribution = texture(noiseTex, uv * 4.0).x * 0.1;
    float distanceFactor = clamp(length(viewPosition) / (length(viewPosition) + 1.0), 0.0, 1.0);

    outOcclusion = clamp(baseOcclusion + noiseContribution + distanceFactor * 0.05, 0.0, 1.0);
}
