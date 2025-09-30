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
    vec4 textureUsage;
};

layout(set = 1, binding = 0) readonly buffer MaterialBuffer {
    MaterialGpu materials[];
} materialBuffer;
layout(set = 1, binding = 1) uniform sampler2D materialTextures[MATERIAL_TEXTURE_COUNT];

struct LightGpu {
    vec4 directionIntensity;
    vec4 colorType;
};

layout(set = 2, binding = 0) readonly buffer LightBuffer {
    LightGpu lights[];
} lightBuffer;

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
    if (material.textureUsage.x > 0.5 && baseIndex < MATERIAL_TEXTURE_COUNT) {
        vec4 sampleBase = texture(materialTextures[baseIndex], fragUV);
        albedo *= sampleBase.rgb;
    }

    uint metallicIndex = material.textureIndices.z;
    if (material.textureUsage.z > 0.5 && metallicIndex < MATERIAL_TEXTURE_COUNT) {
        vec4 mrSample = texture(materialTextures[metallicIndex], fragUV);
        metallic = clamp(mrSample.r, 0.0, 1.0);
        roughness = clamp(mrSample.g, 0.0, 1.0);
    }

    uint aoIndex = material.textureIndices.w;
    if (material.textureUsage.w > 0.5 && aoIndex < MATERIAL_TEXTURE_COUNT) {
        float aoSample = texture(materialTextures[aoIndex], fragUV).r;
        ambientOcclusion = clamp(aoSample, 0.0, 1.0);
    }
    LightGpu keyLight = lightBuffer.lights[0];
    vec3 lightDirection = normalize(-keyLight.directionIntensity.xyz);
    float diffuse = max(dot(normalWorld, lightDirection), 0.0) * keyLight.directionIntensity.w;

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

    vec3 direct = albedo * diffuse * keyLight.colorType.rgb;
    vec3 lit = direct + albedo * ambient;

    outColor = vec4(lit, 1.0);
    outAlbedo = vec4(albedo, 1.0);
    outNormal = vec4(normalView * 0.5 + 0.5, 1.0);
    outLinearDepth = -fragViewPos.z;
}
