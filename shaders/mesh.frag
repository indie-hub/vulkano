#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    vec4 baseColor;
} PC;

void main()
{
    // Simple pass-through color for now (lighting later)
    outColor = PC.baseColor;
}

