#version 450

layout(location = 0) in vec3 vViewNormal;
layout(location = 0) out vec4 outNormal;

void main() {
    vec3 n = normalize(vViewNormal);
    // Pack view-space normal into UNORM (N * 0.5 + 0.5)
    vec3 packed = n * 0.5 + 0.5;
    outNormal = vec4(packed, 1.0);
}

