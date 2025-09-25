#version 450

layout(location = 0) in vec3 vViewPos;
layout(location = 1) in vec3 vViewNormal;

layout(location = 0) out vec4 outViewNormal;
layout(location = 1) out vec4 outViewPosition;

void main() {
    vec3 normal = normalize(vViewNormal);
    outViewNormal = vec4(normal, 0.0);
    outViewPosition = vec4(vViewPos, 1.0);
}
