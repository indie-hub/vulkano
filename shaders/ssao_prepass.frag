#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vWorldNormal;
layout(location = 2) in vec2 vUv;
layout(location = 3) in vec3 vWorldTangent;
layout(location = 4) in vec3 vWorldBitangent;

layout(location = 0) out vec4 outWorldNormal;
layout(location = 1) out vec4 outWorldPosition;

layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;

layout(push_constant) uniform PrimitivePushConstants {
    mat4 model;
    vec4 materialColor;
    vec4 materialProperties;
    vec4 textureControls;
} primitiveConstants;

void main() {
    vec3 normal = normalize(vWorldNormal);
    const bool normalEnabled = primitiveConstants.textureControls.y > 0.5;
    const float normalStrength = max(primitiveConstants.textureControls.z, 0.0);

    if(normalEnabled) {
        vec3 sampledNormal = texture(normalMap, vUv).rgb * 2.0 - 1.0;
        sampledNormal = normalize(vec3(sampledNormal.xy * normalStrength, sampledNormal.z));
        mat3 tbn = mat3(normalize(vWorldTangent), normalize(vWorldBitangent), normalize(vWorldNormal));
        normal = normalize(tbn * sampledNormal);
    }

    outWorldNormal = vec4(normal, 0.0);
    outWorldPosition = vec4(vWorldPos, 1.0);
}
