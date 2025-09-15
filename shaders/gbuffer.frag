#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec2 vUV;
layout(location = 2) in mat3 vTBN;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;

layout(binding = 1) uniform sampler2D uAlbedoMap;
layout(binding = 2) uniform sampler2D uNormalMap;

layout(push_constant) uniform PushConsts {
    mat4 model;
} uPush;

layout(std140, binding = 3) uniform MaterialUBO {
    vec3 albedo;
    float normalStrength;
} uMaterial;

void main() {
    vec3 albedo = texture(uAlbedoMap, vUV).rgb * uMaterial.albedo;
    vec3 n = texture(uNormalMap, vUV).xyz * 2.0 - 1.0;
    n.xy *= uMaterial.normalStrength;
    vec3 normal = normalize(vTBN * normalize(n));
    outAlbedo = vec4(albedo, 1.0);
    outNormal = vec4(normalize(normal) * 0.5 + 0.5, 1.0);
}

