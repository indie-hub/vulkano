#version 450

const uint MAX_SHADOW_CASCADES = 4u;

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUv;
layout(location = 3) in vec3 vTangent;
layout(location = 4) in vec3 vBitangent;

layout(location = 0) out vec4 outColor;

struct ShadowUniform {
    mat4 lightViewProjection[MAX_SHADOW_CASCADES];
    vec4 cascadeData[MAX_SHADOW_CASCADES];
    vec4 cascadeSplits;
    vec4 shadowParams;
    vec4 biasParams;
    vec4 atlasSize;
    vec4 debugParams;
};

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 projection;
    vec4 lightDirectionIntensity;
    vec4 cameraPosition;
    ShadowUniform shadow;
} globalUniforms;

layout(set = 0, binding = 1) uniform sampler2DArray shadowAtlas;

layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;

layout(push_constant) uniform PrimitivePushConstants {
    mat4 model;
    vec4 materialColor;
    vec4 materialProperties;
    vec4 materialFlags;
} primitiveConstants;

const vec3 CASCADE_DEBUG_COLOURS[4] = vec3[4](
    vec3(1.0, 0.4, 0.4),
    vec3(0.4, 1.0, 0.4),
    vec3(0.4, 0.8, 1.0),
    vec3(1.0, 0.9, 0.5)
);

float compute_shadow(uint cascadeIndex, vec3 worldPosition, vec3 normal) {
    float cascadesEnabled = globalUniforms.shadow.shadowParams.x;
    if(cascadesEnabled < 0.5) {
        return 0.0;
    }

    vec4 cascadeMeta = globalUniforms.shadow.cascadeData[cascadeIndex];
    float mapResolution = globalUniforms.shadow.atlasSize.x;
    float invMapResolution = globalUniforms.shadow.atlasSize.y;

    vec3 lightDir = normalize(-globalUniforms.lightDirectionIntensity.xyz);
    float ndotl = clamp(dot(normal, lightDir), 0.0, 1.0);
    float angularFactor = 1.0 - ndotl;

    float texelSize = max(cascadeMeta.x, 1e-4);
    float normalBiasBase = globalUniforms.shadow.biasParams.z * texelSize;
    float normalBias = normalBiasBase * (1.0 + angularFactor);
    vec3 biasedPosition = worldPosition + normal * normalBias;

    vec4 lightSpace = globalUniforms.shadow.lightViewProjection[cascadeIndex] * vec4(biasedPosition, 1.0);
    vec3 shadowCoord = lightSpace.xyz / lightSpace.w;
    shadowCoord = shadowCoord * 0.5 + 0.5;

    if(shadowCoord.z > 1.0 || shadowCoord.z < 0.0) {
        return 0.0;
    }

    int kernelRadius = int(round(globalUniforms.shadow.shadowParams.z));
    kernelRadius = max(kernelRadius, 0);
    float constantBias = globalUniforms.shadow.biasParams.x;
    float slopeBias = globalUniforms.shadow.biasParams.y * angularFactor;
    float depthBias = (constantBias + slopeBias) * texelSize;

    float shadow = 0.0;
    float samples = 0.0;

    for(int x = -kernelRadius; x <= kernelRadius; ++x) {
        for(int y = -kernelRadius; y <= kernelRadius; ++y) {
            vec2 offset = vec2(float(x), float(y)) * invMapResolution;
            float closestDepth = texture(shadowAtlas, vec3(shadowCoord.xy + offset, float(cascadeIndex))).r;
            float currentDepth = shadowCoord.z - depthBias;
            shadow += closestDepth < currentDepth ? 1.0 : 0.0;
            samples += 1.0;
        }
    }

    if(samples <= 0.0) {
        return 0.0;
    }
    return shadow / samples;
}

uint select_cascade(float viewSpaceDepth, uint cascadeCount) {
    uint cascadeIndex = cascadeCount - 1u;
    for(uint index = 0u; index < cascadeCount; ++index) {
        if(viewSpaceDepth <= globalUniforms.shadow.cascadeSplits[index]) {
            cascadeIndex = index;
            break;
        }
    }
    return cascadeIndex;
}

void main() {
    vec3 normal = normalize(vNormal);
    vec3 baseColor = primitiveConstants.materialColor.rgb;

    if(primitiveConstants.materialFlags.x > 0.5) {
        vec4 sampledAlbedo = texture(albedoMap, vUv);
        baseColor *= sampledAlbedo.rgb;
    }

    if(primitiveConstants.materialFlags.y > 0.5) {
        vec3 tangent = normalize(vTangent);
        vec3 bitangent = normalize(vBitangent);
        mat3 tbn = mat3(tangent, bitangent, normal);
        vec3 sampledNormal = texture(normalMap, vUv).xyz * 2.0 - 1.0;
        sampledNormal.xy *= primitiveConstants.materialFlags.z;
        sampledNormal = normalize(sampledNormal);
        normal = normalize(tbn * sampledNormal);
    }

    vec3 lightDirection = normalize(-globalUniforms.lightDirectionIntensity.xyz);
    float lightIntensity = globalUniforms.lightDirectionIntensity.w;

    vec3 viewPosition = vec3(globalUniforms.view * vec4(vWorldPos, 1.0));
    float viewDepth = -viewPosition.z;

    uint cascadeCount = uint(globalUniforms.shadow.shadowParams.w);
    cascadeCount = max(cascadeCount, 1u);
    cascadeCount = min(cascadeCount, MAX_SHADOW_CASCADES);
    uint cascadeIndex = select_cascade(viewDepth, cascadeCount);

    float NdotL = max(dot(normal, lightDirection), 0.0);

    float ambientStrength = primitiveConstants.materialProperties.y;
    float specularStrength = primitiveConstants.materialProperties.z;
    float shininess = primitiveConstants.materialProperties.x;

    vec3 ambient = ambientStrength * lightIntensity * baseColor;
    vec3 diffuse = NdotL * lightIntensity * baseColor;

    vec3 viewDirection = normalize(globalUniforms.cameraPosition.xyz - vWorldPos);
    vec3 halfwayDirection = normalize(lightDirection + viewDirection);
    float specAngle = max(dot(normal, halfwayDirection), 0.0);
    float specularFactor = specularStrength * lightIntensity * pow(specAngle, shininess);
    vec3 specular = specularFactor * vec3(1.0);

    float shadowAmount = compute_shadow(cascadeIndex, vWorldPos, normal);
    float shadowStrength = clamp(globalUniforms.shadow.shadowParams.y, 0.0, 1.0);
    float lightVisibility = mix(1.0, 1.0 - shadowAmount, shadowStrength);

    vec3 lighting = ambient + (diffuse + specular) * lightVisibility;
    vec3 colour = lighting;

    if(globalUniforms.shadow.debugParams.x > 0.5) {
        vec3 cascadeColour = CASCADE_DEBUG_COLOURS[int(cascadeIndex) % 4];
        colour = mix(colour, cascadeColour, 0.35);
    }

    outColor = vec4(colour, primitiveConstants.materialColor.a);
}
