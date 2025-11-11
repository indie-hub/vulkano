#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormalWorld;
layout(location = 2) in vec3 fragViewPos;
layout(location = 3) in vec3 fragNormalView;
layout(location = 4) in vec2 fragUV;
layout(location = 5) flat in uint fragMaterialIndex;

layout(set = 0, binding = 0) uniform SSAOConfig {
    float occlusionStrength;
    float baseAmbient;
    float debugView;
    float padding;
} uConfig;
layout(set = 0, binding = 1) uniform sampler2D ssaoTex;

const uint MATERIAL_TEXTURE_COUNT = 12u;

struct MaterialGpu {
    vec4 baseColorMetallic;
    vec4 roughnessAoFlags;
    uvec4 textureIndices;
};

layout(set = 1, binding = 0) readonly buffer MaterialBuffer {
    MaterialGpu materials[];
} materialBuffer;
layout(set = 1, binding = 1) uniform sampler2D materialTextures[MATERIAL_TEXTURE_COUNT];

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outAlbedo;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out float outLinearDepth;

void main() {
    vec3 normalWorld = normalize(fragNormalWorld);
    vec3 normalView = normalize(fragNormalView);

    MaterialGpu material = materialBuffer.materials[fragMaterialIndex];
    vec3 albedo = material.baseColorMetallic.rgb;
    float metallic = material.baseColorMetallic.a;
    float roughness = material.roughnessAoFlags.x;
    float ambientOcclusion = material.roughnessAoFlags.y;

    uint baseIndex = material.textureIndices.x;
    if (baseIndex < MATERIAL_TEXTURE_COUNT) {
        vec4 sampleBase = texture(materialTextures[baseIndex], fragUV);
        albedo *= sampleBase.rgb;
    }
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diffuse = max(dot(normalWorld, lightDir), 0.0);

    vec2 occlusionUV = gl_FragCoord.xy / vec2(textureSize(ssaoTex, 0));
    float occlusion = texture(ssaoTex, occlusionUV).r;

    if (uConfig.debugView > 0.5) {
        vec3 debugColor = vec3(occlusion);
        outColor = vec4(debugColor, 1.0);
        outAlbedo = vec4(albedo, 1.0);
        outNormal = vec4(normalView * 0.5 + 0.5, 1.0);
        outLinearDepth = -fragViewPos.z;
        return;
    }

    float occlusionFactor = mix(1.0, occlusion, clamp(uConfig.occlusionStrength, 0.0, 1.0));
    float ambient = uConfig.baseAmbient * occlusionFactor * ambientOcclusion;

    vec3 lit = albedo * (diffuse + ambient);

    outColor = vec4(lit, 1.0);
    outAlbedo = vec4(albedo, 1.0);
    outNormal = vec4(normalView * 0.5 + 0.5, 1.0);
    outLinearDepth = -fragViewPos.z;
}
