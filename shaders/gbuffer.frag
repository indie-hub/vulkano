#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec2 vUV;
layout(location = 2) in mat3 vTBN;
layout(location = 5) in vec3 vViewPos;

layout(location = 0) out vec4 outAlbedo; // lit color
layout(location = 1) out vec4 outNormal; // normal encoded in [0,1]
layout(location = 2) out float outViewZ; // positive view-space depth

layout(binding = 1) uniform sampler2D uAlbedoMap;
layout(binding = 2) uniform sampler2D uNormalMap;

layout(push_constant) uniform PushConsts {
    mat4 model;
} uPush;

layout(std140, binding = 3) uniform MaterialUBO {
    vec3 albedo;
    float normalStrength;
} uMaterial;

layout(std140, binding = 4) uniform LightUBO {
    vec3 lightPos;
    float intensity;
    vec3 lightColor;
    float shininess;
} uLight;

void main() {
    vec3 baseAlbedo = texture(uAlbedoMap, vUV).rgb * uMaterial.albedo;
    vec3 n = texture(uNormalMap, vUV).xyz * 2.0 - 1.0;
    n.xy *= uMaterial.normalStrength;
    vec3 N = normalize(vTBN * normalize(n));
    vec3 fragPos = vWorldPos;
    vec3 L = normalize(uLight.lightPos - fragPos);
    vec3 V = normalize(-vViewPos);
    vec3 H = normalize(L + V);
    float NdotL = max(dot(N, L), 0.0);
    float diffuse = NdotL;
    float spec = pow(max(dot(N, H), 0.0), uLight.shininess);
    vec3 color = baseAlbedo * diffuse + spec * uLight.lightColor * uLight.intensity;
    outAlbedo = vec4(color, 1.0);
    outNormal = vec4(normalize(N) * 0.5 + 0.5, 1.0);
    outViewZ = -vViewPos.z;
}
