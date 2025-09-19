#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec3 vWorldPos;
layout(location = 3) in vec3 vTangent;
layout(location = 4) in vec3 vBitangent;

layout(location = 0) out vec4 outNormalVS;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    float ambientStrength; // unused here
    vec3 lightPos;         // unused here
    float lightIntensity;  // unused here
    vec3 lightColor;       // unused here
    float _pad0;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec3 baseColor;   // unused
    float shininess;  // unused
    int useAlbedoMap; // unused
    int useNormalMap;
    float normalStrength;
    float _pad1;
} PC;

layout(set = 0, binding = 2) uniform sampler2D normalMap;

void main()
{
    vec3 Nw = normalize(vNormal);
    if (PC.useNormalMap != 0) {
        vec3 T = normalize(vTangent);
        vec3 B = normalize(vBitangent);
        vec3 n = texture(normalMap, vUV).xyz * 2.0 - 1.0;
        n.xy *= PC.normalStrength;
        n.z = sqrt(max(0.0, 1.0 - dot(n.xy, n.xy)));
        mat3 TBN = mat3(T, B, Nw);
        Nw = normalize(TBN * n);
    }
    // Transform world-space normal to view-space
    mat3 V = mat3(ubo.view);
    vec3 Nv = normalize(V * Nw);
    outNormalVS = vec4(Nv, 0.0);
}

