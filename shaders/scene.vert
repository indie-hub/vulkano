#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragViewPos;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 projection;
} pushConstants;

void main() {
    mat4 model = pushConstants.model;
    mat4 view = pushConstants.view;
    mat4 projection = pushConstants.projection;
    mat3 normalMatrix = mat3(transpose(inverse(model)));

    fragColor = inColor;
    fragNormal = normalMatrix * inNormal;

    vec4 worldPosition = model * vec4(inPosition, 1.0);
    vec4 viewPosition = view * worldPosition;
    fragViewPos = viewPosition.xyz;

    gl_Position = projection * viewPosition;
}
