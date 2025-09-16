#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUV;
layout(location = 2) out vec3 vWorldPos;

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
    vec3 baseColor; // not used in VS
    float shininess; // not used in VS
} PC;

void main()
{
    mat3 normalMat = mat3(transpose(inverse(PC.model)));
    vNormal = normalize(normalMat * inNormal);
    vUV = inUV;
    vec4 worldPos = PC.model * vec4(inPos, 1.0);
    vWorldPos = worldPos.xyz;
    gl_Position = ubo.proj * ubo.view * worldPos;
}
