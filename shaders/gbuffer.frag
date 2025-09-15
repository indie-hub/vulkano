#version 450

layout(location = 0) in vec3 vNormalVS;
layout(location = 1) in float vDepthVS;
layout(location = 2) in vec2 vUV;

layout(location = 0) out vec4 outND;

void main() {
    // Pack view-space normal into RGB ([-1,1] -> [0,1]) and linear depth into A
    vec3 n = normalize(vNormalVS);
    vec3 n01 = n * 0.5 + 0.5;
    float depth = vDepthVS;
    outND = vec4(n01, depth);
}

