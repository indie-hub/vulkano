#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 projection;
} pushConstants;

void main() {
    fragColor = inColor;
    gl_Position = pushConstants.projection * pushConstants.view * pushConstants.model * vec4(inPosition, 1.0);
}
