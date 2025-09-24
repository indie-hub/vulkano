#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUv;
layout(location = 3) in vec4 vLightSpacePos;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 projection;
    mat4 lightViewProjection;
    vec4 lightPositionIntensity;
    vec4 cameraPosition;
} globalUniforms;

layout(set = 0, binding = 1) uniform sampler2DShadow shadowMap;

layout(push_constant) uniform PrimitivePushConstants {
    mat4 model;
    vec4 materialColor;
    vec4 materialProperties; // x: shininess, y: ambientStrength, z: specularStrength, w: unused
} primitiveConstants;

const vec3 UNIT_NORMAL = vec3(0.0, 1.0, 0.0);

void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightPosition = globalUniforms.lightPositionIntensity.xyz;
    float lightIntensity = globalUniforms.lightPositionIntensity.w;
    vec3 lightDirection = normalize(lightPosition - vWorldPos);

    float diffuseStrength = max(dot(normal, lightDirection), 0.0);
    vec3 baseColor = primitiveConstants.materialColor.rgb;

    float ambientStrength = primitiveConstants.materialProperties.y;
    float specularStrength = primitiveConstants.materialProperties.z;
    float shininess = primitiveConstants.materialProperties.x;

    vec3 ambient = ambientStrength * lightIntensity * baseColor;

    vec3 shadowCoord = vLightSpacePos.xyz / vLightSpacePos.w;
    shadowCoord = shadowCoord * 0.5 + 0.5;
    float visibility = 1.0;

    if(shadowCoord.x >= 0.0 && shadowCoord.x <= 1.0 && shadowCoord.y >= 0.0 && shadowCoord.y <= 1.0 && shadowCoord.z >= 0.0 && shadowCoord.z <= 1.0) {
        vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
        float normalDotLight = max(dot(normal, lightDirection), 0.0);
        float bias = max(0.0025, 0.05 * (1.0 - normalDotLight));
        float sum = 0.0;
        const int kernelRadius = 1;
        for(int x = -kernelRadius; x <= kernelRadius; ++x) {
            for(int y = -kernelRadius; y <= kernelRadius; ++y) {
                vec2 offset = vec2(x, y) * texelSize;
                sum += texture(shadowMap, vec3(shadowCoord.xy + offset, shadowCoord.z - bias));
            }
        }
        visibility = sum / 9.0;
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
