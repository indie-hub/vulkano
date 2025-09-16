#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec3 vWorldPos;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    float ambientStrength;
    vec3 lightPos;
    float lightIntensity;
    vec3 lightColor;
    float _pad0;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec3 baseColor;
    float shininess;
} PC;

void main()
{
    // Blinn-Phong shading
    vec3 N = normalize(vNormal);
    vec3 L = normalize(ubo.lightPos - vWorldPos);
    vec3 V = normalize(ubo.cameraPos - vWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = 0.0;
    if (diff > 0.0) {
        spec = pow(max(dot(N, H), 0.0), max(PC.shininess, 1.0));
    }

    vec3 ambient = ubo.ambientStrength * PC.baseColor;
    vec3 diffuse = diff * PC.baseColor * ubo.lightColor * ubo.lightIntensity;
    vec3 specular = spec * ubo.lightColor * ubo.lightIntensity;
    vec3 color = ambient + diffuse + specular;
    outColor = vec4(color, 1.0);
}
