#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUv;
layout(location = 3) in vec3 vTangent;
layout(location = 4) in vec3 vBitangent;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 projection;
    mat4 lightViewProjection[4];
    vec4 lightPositionIntensity;
    vec4 cameraPosition;
    vec4 shadowParams;
    vec4 shadowConfig;
    vec4 cascadeSplits;
    vec4 cameraClip;
} globalUniforms;

layout(set = 0, binding = 1) uniform sampler2DArray shadowMap;

layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;

layout(push_constant) uniform PrimitivePushConstants {
    mat4 model;
    vec4 materialColor;
    vec4 materialProperties; // x: shininess, y: ambientStrength, z: specularStrength, w: unused
    vec4 textureControls; // x: texturesEnabled, y: normalStrength
} primitiveConstants;

vec3 srgb_to_linear(vec3 value) {
    return pow(value, vec3(2.2));
}

void main() {
    vec3 lightPosition = globalUniforms.lightPositionIntensity.xyz;
    float lightIntensity = globalUniforms.lightPositionIntensity.w;
    vec3 lightDirection = normalize(lightPosition - vWorldPos);

    vec3 normal = normalize(vNormal);
    vec3 tangent = normalize(vTangent);
    vec3 bitangent = normalize(vBitangent);
    mat3 tbn = mat3(tangent, bitangent, normal);

    vec3 baseColor = primitiveConstants.materialColor.rgb;
    const bool texturesEnabled = primitiveConstants.textureControls.x > 0.5;
    if(texturesEnabled) {
        vec3 albedoSample = texture(albedoMap, vUv).rgb;
        baseColor *= srgb_to_linear(albedoSample);

        vec3 sampledNormal = texture(normalMap, vUv).rgb * 2.0 - 1.0;
        float normalStrength = max(primitiveConstants.textureControls.y, 0.0);
        sampledNormal = normalize(vec3(sampledNormal.xy * normalStrength, sampledNormal.z));
        normal = normalize(tbn * sampledNormal);
    }

    float diffuseStrength = max(dot(normal, lightDirection), 0.0);

    float ambientStrength = primitiveConstants.materialProperties.y;
    float specularStrength = primitiveConstants.materialProperties.z;
    float shininess = primitiveConstants.materialProperties.x;

    vec3 ambient = ambientStrength * lightIntensity * baseColor;

    vec4 viewPosition = globalUniforms.view * vec4(vWorldPos, 1.0);
    float viewDepth = abs(viewPosition.z);
    float nearPlane = globalUniforms.cameraClip.x;
    float farPlane = globalUniforms.cameraClip.y;
    float clipRange = max(globalUniforms.cameraClip.z, 0.0001);
    float depthNormalized = clamp((viewDepth - nearPlane) / clipRange, 0.0, 1.0);

    if(globalUniforms.shadowConfig.x < 0.5) {
        vec3 diffuse = diffuseStrength * lightIntensity * baseColor;
        vec3 viewDirection = normalize(globalUniforms.cameraPosition.xyz - vWorldPos);
        vec3 halfwayDirection = normalize(lightDirection + viewDirection);
        float specAngle = max(dot(normal, halfwayDirection), 0.0);
        float specularFactor = specularStrength * lightIntensity * pow(specAngle, shininess);
        vec3 specular = specularFactor * vec3(1.0);
        vec3 finalColor = ambient + diffuse + specular;
        outColor = vec4(finalColor, primitiveConstants.materialColor.a);
        return;
    }

    int cascadeCount = int(round(clamp(globalUniforms.shadowConfig.y, 1.0, 4.0)));
    int cascadeIndex = cascadeCount - 1;
    for(int index = 0; index < cascadeCount; ++index) {
        if(depthNormalized <= globalUniforms.cascadeSplits[index]) {
            cascadeIndex = index;
            break;
        }
    }

    mat4 lightMatrix = globalUniforms.lightViewProjection[cascadeIndex];
    vec4 lightSpacePos = lightMatrix * vec4(vWorldPos, 1.0);
    vec3 shadowCoord = lightSpacePos.xyz / lightSpacePos.w;
    shadowCoord = shadowCoord * 0.5 + 0.5;
    float visibility = 1.0;

    if(shadowCoord.x >= 0.0 && shadowCoord.x <= 1.0 && shadowCoord.y >= 0.0 && shadowCoord.y <= 1.0 && shadowCoord.z >= 0.0 && shadowCoord.z <= 1.0) {
        ivec3 dimensions = textureSize(shadowMap, 0);
        vec2 texelSize = 1.0 / vec2(dimensions.xy);
        float normalDotLight = max(dot(normal, lightDirection), 0.0);
        float biasBase = globalUniforms.shadowParams.x;
        float biasFactor = globalUniforms.shadowParams.y;
        int kernelRadius = int(round(clamp(globalUniforms.shadowParams.z, 0.0, 4.0)));
        float bias = max(biasBase, biasFactor * (1.0 - normalDotLight));
        int diameter = (kernelRadius * 2) + 1;
        int sampleCount = max(diameter * diameter, 1);
        float sum = 0.0;
        for(int x = -kernelRadius; x <= kernelRadius; ++x) {
            for(int y = -kernelRadius; y <= kernelRadius; ++y) {
                vec2 offset = vec2(x, y) * texelSize;
                float sampledDepth = texture(shadowMap, vec3(shadowCoord.xy + offset, float(cascadeIndex))).r;
                sum += sampledDepth >= (shadowCoord.z - bias) ? 1.0 : 0.0;
            }
        }
        visibility = sum / float(sampleCount);
    }

    vec3 diffuse = diffuseStrength * lightIntensity * baseColor * visibility;

    vec3 viewDirection = normalize(globalUniforms.cameraPosition.xyz - vWorldPos);
    vec3 halfwayDirection = normalize(lightDirection + viewDirection);
    float specAngle = max(dot(normal, halfwayDirection), 0.0);
    float specularFactor = specularStrength * lightIntensity * pow(specAngle, shininess);
    vec3 specular = specularFactor * vec3(1.0) * visibility;

    vec3 finalColor = ambient + diffuse + specular;
    outColor = vec4(finalColor, primitiveConstants.materialColor.a);
}
