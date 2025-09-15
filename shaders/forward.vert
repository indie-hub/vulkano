#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inBitangent;
layout(location = 4) in vec2 inUV;

layout(push_constant) uniform PC {
    mat4 mvp;
} pc;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec3 vTangent;
layout(location = 2) out vec3 vBitangent;
layout(location = 3) out vec2 vUV;
layout(location = 4) out vec3 vPosition;

void main() {
    gl_Position = pc.mvp * vec4(inPos, 1.0);
    vNormal = inNormal;
    vTangent = inTangent;
    vBitangent = inBitangent;
    vUV = inUV;
    vPosition = inPos;
}

