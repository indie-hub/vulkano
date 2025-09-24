#version 450

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform ShadowPushConstants {
    mat4 model;
    mat4 lightViewProjection;
} shadowConstants;

void main() {
    gl_Position = shadowConstants.lightViewProjection * shadowConstants.model * vec4(inPosition, 1.0);
}

