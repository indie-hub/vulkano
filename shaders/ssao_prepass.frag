#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vWorldNormal;

layout(location = 0) out vec4 outWorldNormal;
layout(location = 1) out vec4 outWorldPosition;

void main() {
    vec3 normal = normalize(vWorldNormal);
    outWorldNormal = vec4(normal, 0.0);
    outWorldPosition = vec4(vWorldPos, 1.0);
}
