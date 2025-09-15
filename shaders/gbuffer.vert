#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec2 vUV;
layout(location = 2) out mat3 vTBN;
layout(location = 5) out vec3 vViewPos;

layout(std140, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 cameraPos;
} uCamera;

layout(push_constant) uniform PushConsts {
    mat4 model;
} uPush;

void main() {
    vec3 N = normalize(inNormal);
    vec3 T = normalize(inTangent);
    vec3 B = normalize(inBitangent);
    vTBN = mat3(T, B, N);
    vec4 world = uPush.model * vec4(inPosition, 1.0);
    vWorldPos = world.xyz;
    vViewPos = (uCamera.view * world).xyz;
    vUV = inUV;
    gl_Position = uCamera.viewProj * world;
}
