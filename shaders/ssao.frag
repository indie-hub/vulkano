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
    mat4 projection;
    vec4 sampleParams; // xy: noise scale, z: radius, w: bias
    vec4 attenuationParams; // x: angle cos threshold, y: depth falloff, z: distance falloff, w: padding
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

    if (linearDepth <= 0.0) {
        outOcclusion = 1.0;
        return;
    }

    vec2 noiseScale = uParams.sampleParams.xy;
    float radius = uParams.sampleParams.z;
    float bias = uParams.sampleParams.w;

    vec3 randomVec = normalize(texture(noiseTex, uv * noiseScale).xyz * 2.0 - 1.0);
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tbn = mat3(tangent, bitangent, normal);

    float occlusionAccum = 0.0;
    float weightAccum = 0.0;

    float angleCosThreshold = clamp(uParams.attenuationParams.x, -1.0, 1.0);
    float depthFalloff = max(uParams.attenuationParams.y, 0.01);
    float distanceFalloff = max(uParams.attenuationParams.z, 0.01);

    const int sampleCount = 64;
    for (int i = 0; i < sampleCount; ++i) {
    vec3 sampleVec = tbn * uKernel.samples[i].xyz;
    sampleVec = viewPosition + sampleVec * radius;

    vec4 sampleClip = uParams.projection * vec4(sampleVec, 1.0);
    vec3 sampleNdc = sampleClip.xyz / sampleClip.w;
    vec2 sampleUV = sampleNdc.xy * 0.5 + 0.5;

    if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) {
        continue;
    }

    float sampleDepth = texture(depthTex, sampleUV).r;
    if (sampleDepth <= 0.0) {
        continue;
    }

    vec3 occluderPosition = reconstruct_view_position(sampleDepth, sampleUV);
    vec3 occluderOffset = occluderPosition - viewPosition;
    float occluderDistance = length(occluderOffset);
    if (occluderDistance <= 1e-4) {
        continue;
    }

    vec3 occluderDir = occluderOffset / occluderDistance;
    float angleCos = dot(normal, occluderDir);
    if (angleCos < angleCosThreshold) {
        continue;
    }

    vec3 occluderNormal = normalize(texture(normalTex, sampleUV).xyz * 2.0 - 1.0);
    float normalEpsilon = clamp(uParams.attenuationParams.w, 0.0, 1.0);
    if (normalEpsilon > 0.0 && dot(normal, occluderNormal) > 1.0 - normalEpsilon) {
        continue;
    }

    float fragmentDepth = linearDepth;
    float depthDifference = fragmentDepth - sampleDepth - bias;
    if (depthDifference <= 0.0) {
        continue;
    }

    float depthWeight = exp(-depthDifference * depthFalloff);
    float distanceWeight = exp(-occluderDistance * distanceFalloff);
    float weight = depthWeight * distanceWeight;

    occlusionAccum += weight;
    weightAccum += weight;
    }

    float occlusion = weightAccum > 0.0 ? occlusionAccum / weightAccum : 0.0;
    outOcclusion = clamp(1.0 - occlusion, 0.0, 1.0);
}
