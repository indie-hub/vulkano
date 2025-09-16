#version 450

layout(location = 0) in vec2 inPos;

layout(push_constant) uniform Constants {
    vec4 color;
} pc;

void main() {
    gl_Position = vec4(inPos, 0.0, 1.0);
}

