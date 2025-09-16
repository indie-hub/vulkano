#version 450

layout(location = 0) in vec2 inPos;

layout(push_constant) uniform PushConstants {
    vec4 color; // not used in VS, present for layout compatibility
} PC;

void main()
{
    gl_Position = vec4(inPos, 0.0, 1.0);
}

