#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec2 vUV;
layout(location = 2) in mat3 vTBN;

layout(std140, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 cameraPos;
} uCamera;

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

layout(binding = 1) uniform sampler2D uAlbedoMap;
layout(binding = 2) uniform sampler2D uNormalMap;

void main() {
    vec3 baseAlbedo = texture(uAlbedoMap, vUV).rgb * uMaterial.albedo;
    vec3 n = texture(uNormalMap, vUV).xyz * 2.0 - 1.0;
    n.xy *= uMaterial.normalStrength;
    vec3 N = normalize(vTBN * normalize(n));
    vec3 fragPos = vWorldPos;
    vec3 L = normalize(uLight.lightPos - fragPos);
    vec3 V = normalize(uCamera.cameraPos - fragPos);
    vec3 H = normalize(L + V);
    float NdotL = max(dot(N, L), 0.0);
    float diffuse = NdotL;
    float spec = pow(max(dot(N, H), 0.0), uLight.shininess);
    vec3 color = baseAlbedo * diffuse + spec * uLight.lightColor * uLight.intensity;
    color = pow(color, vec3(1.0/2.2));
    outColor = vec4(color, 1.0);
}
