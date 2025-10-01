#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormalWorld;
layout(location = 2) in vec3 fragViewPos;
layout(location = 3) in vec3 fragNormalView;
layout(location = 4) in vec2 fragUV;
layout(location = 5) in vec3 fragPosWorld;
layout(location = 6) flat in uint fragMaterialIndex;
layout(location = 7) in vec3 fragTangentWorld;
layout(location = 8) flat in float fragBitangentSign;

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
    vec4 roughnessAoStrength;
    uvec4 textureIndices;
    vec4 textureUsage;
    vec4 emissive;
};

layout(set = 1, binding = 0) readonly buffer MaterialBuffer {
    MaterialGpu materials[];
} materialBuffer;
layout(set = 1, binding = 1) uniform sampler2D materialTextures[MATERIAL_TEXTURE_COUNT];

struct LightGpu {
    vec4 directionIntensity;
    vec4 colorType;
    vec4 positionRange;
    vec4 shadowParams;
};

layout(set = 2, binding = 0) readonly buffer LightBuffer {
    LightGpu lights[];
} lightBuffer;
layout(set = 3, binding = 0) uniform sampler2D shadowMaps[3];
layout(set = 3, binding = 1) readonly buffer ShadowMatrices {
    mat4 matrices[3];
    uvec4 meta;
} shadowData;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 lightViewProjection;
    uvec4 materialData;
    vec4 cameraPosition;
    vec4 shadowParams;
} pushConstants;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outAlbedo;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out float outLinearDepth;

void main() {
    const float PI = 3.14159265359;

    vec3 normalWorld = normalize(fragNormalWorld);
    vec3 tangentWorld = normalize(fragTangentWorld);
    if (length(tangentWorld) < 1e-6) {
        tangentWorld = normalize(cross(normalWorld, vec3(0.0, 1.0, 0.0)));
        if (length(tangentWorld) < 1e-6) {
            tangentWorld = vec3(1.0, 0.0, 0.0);
        }
    }
    vec3 bitangentWorld = normalize(cross(normalWorld, tangentWorld)) * (fragBitangentSign >= 0.0 ? 1.0 : -1.0);
    if (length(bitangentWorld) < 1e-6) {
        bitangentWorld = normalize(cross(normalWorld, tangentWorld));
    }

    MaterialGpu material = materialBuffer.materials[fragMaterialIndex];
    vec3 albedo = material.baseColorMetallic.rgb;
    float metallic = material.baseColorMetallic.a;
    float roughness = material.roughnessAoStrength.x;
    float ambientOcclusion = material.roughnessAoStrength.y;

    uint normalIndex = material.textureIndices.y;
    float normalStrength = material.roughnessAoStrength.z;
    normalStrength = clamp(normalStrength, 0.0, 1.0);

    if (material.textureUsage.y > 0.5 && normalIndex < MATERIAL_TEXTURE_COUNT) {
        vec3 normalSample = texture(materialTextures[normalIndex], fragUV).rgb;
        vec3 normalTangent = normalize(normalSample * 2.0 - 1.0);
        normalTangent = normalize(mix(vec3(0.0, 0.0, 1.0), normalTangent, normalStrength));
        mat3 tbn = mat3(tangentWorld, bitangentWorld, normalWorld);
        normalWorld = normalize(tbn * normalTangent);
    }
    vec3 normalView = normalize(mat3(pushConstants.view) * normalWorld);

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

    vec3 V = normalize(pushConstants.cameraPosition.xyz - fragPosWorld);
    if (length(V) < 1e-6) {
        V = normalize(-fragViewPos);
    }
    float NdotV = max(dot(normalWorld, V), 0.0);

    bool shadowEnabled = pushConstants.shadowParams.z > 0.5;
    float shadowBias = pushConstants.shadowParams.x;
    float shadowRadius = pushConstants.shadowParams.y;
    vec3 radiance = vec3(0.0);
    if (NdotV > 0.0) {
        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        const uint lightCount = lightBuffer.lights.length();
        for (uint i = 0u; i < lightCount; ++i) {
            LightGpu light = lightBuffer.lights[i];
            float attenuation = light.directionIntensity.w;
            vec3 L;

            float typeValue = light.colorType.w;
            bool castsShadowLight = light.shadowParams.x > 0.5;
            int typeIndex = int(typeValue) & 1;
            bool isDirectional = typeIndex == 0;
            if (isDirectional) {
                L = normalize(-light.directionIntensity.xyz);
            } else {
                // Point
                vec3 toLight = light.positionRange.xyz - fragPosWorld;
                float distance = length(toLight);
                if (distance <= 1e-4) {
                    continue;
                }
                float range = max(light.positionRange.w, 0.01);
                float falloff = clamp(1.0 - distance / range, 0.0, 1.0);
                attenuation *= falloff * falloff;
                L = toLight / distance;
            }

            float NdotL = max(dot(normalWorld, L), 0.0);
            if (NdotL <= 0.0) {
                continue;
            }

            vec3 H = normalize(L + V);
            float NdotH = max(dot(normalWorld, H), 0.0);
            float VdotH = max(dot(V, H), 0.0);

            float alpha = max(roughness * roughness, 0.001);
            float alpha2 = alpha * alpha;
            float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
            float D = alpha2 / (PI * denom * denom);

            float k = (roughness + 1.0);
            k = (k * k) * 0.125;
            float Gv = NdotV / (NdotV * (1.0 - k) + k);
            float Gl = NdotL / (NdotL * (1.0 - k) + k);
            float G = Gv * Gl;

            vec3 F = F0 + (1.0 - F0) * pow(max(1.0 - VdotH, 0.0), 5.0);

            vec3 numerator = D * G * F;
            float denomSpec = max(4.0 * NdotV * NdotL, 0.001);
            vec3 specular = numerator / denomSpec;

            vec3 kd = (1.0 - F) * (1.0 - metallic);
            vec3 diffuse = kd * albedo / PI;

            vec3 lightColor = light.colorType.rgb * attenuation;

            float shadowFactor = 1.0;
            uint shadowIndex = uint(light.shadowParams.y);
            uint activeCount = shadowData.meta.x;
            bool validShadowIndex = shadowIndex < activeCount;

            if (shadowEnabled && isDirectional && castsShadowLight && validShadowIndex) {
                vec4 lightSpace = shadowData.matrices[shadowIndex] * vec4(fragPosWorld, 1.0);
                float invWS = lightSpace.w != 0.0 ? 1.0 / lightSpace.w : 0.0;
                vec3 shadowCoord = lightSpace.xyz * invWS;
                shadowCoord = shadowCoord * 0.5 + 0.5;
                bool shadowCoordValid = shadowCoord.x >= 0.0 && shadowCoord.x <= 1.0 && shadowCoord.y >= 0.0
                    && shadowCoord.y <= 1.0 && shadowCoord.z <= 1.0;
                if (shadowCoordValid) {
                    ivec2 texSize = textureSize(shadowMaps[int(shadowIndex)], 0);
                    if (texSize.x > 0 && texSize.y > 0) {
                        vec2 texelSize = 1.0 / vec2(texSize);
                        int samples = int(max(shadowRadius, 0.0));
                        float occluded = 0.0;
                        float total = 0.0;
                        for (int x = -samples; x <= samples; ++x) {
                            for (int y = -samples; y <= samples; ++y) {
                                vec2 offset = vec2(x, y) * texelSize;
                                float closestDepth = texture(shadowMaps[int(shadowIndex)], shadowCoord.xy + offset).r;
                                float currentDepth = shadowCoord.z - shadowBias;
                                occluded += currentDepth > closestDepth ? 1.0 : 0.0;
                                total += 1.0;
                            }
                        }
                        if (total > 0.0) {
                            shadowFactor = 1.0 - occluded / total;
                        }
                    }
                }
            }

            radiance += (diffuse + specular) * lightColor * NdotL * shadowFactor;
        }
    }

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

    vec3 emissive = material.emissive.rgb * material.emissive.w;
    vec3 lit = radiance + albedo * ambient + emissive;

    if (pushConstants.shadowParams.w > 0.5) {
        float depthSample = 0.0;
        if (shadowData.meta.x > 0u) {
            vec4 debugLightSpace = shadowData.matrices[0] * vec4(fragPosWorld, 1.0);
            float invWD = debugLightSpace.w != 0.0 ? 1.0 / debugLightSpace.w : 0.0;
            vec3 debugCoord = debugLightSpace.xyz * invWD;
            debugCoord = debugCoord * 0.5 + 0.5;
            bool debugValid = debugCoord.x >= 0.0 && debugCoord.x <= 1.0 && debugCoord.y >= 0.0 && debugCoord.y <= 1.0;
            if (debugValid) {
                depthSample = texture(shadowMaps[0], debugCoord.xy).r;
            }
        }
        outColor = vec4(vec3(depthSample), 1.0);
        outAlbedo = vec4(albedo, 1.0);
        outNormal = vec4(normalView * 0.5 + 0.5, 1.0);
        outLinearDepth = -fragViewPos.z;
        return;
    }

    outColor = vec4(lit, 1.0);
    outAlbedo = vec4(albedo, 1.0);
    outNormal = vec4(normalView * 0.5 + 0.5, 1.0);
    outLinearDepth = -fragViewPos.z;
}
