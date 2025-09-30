#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormalWorld;
layout(location = 2) out vec3 fragViewPos;
layout(location = 3) out vec3 fragNormalView;

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

    vec3 normalWorld = normalize(normalMatrix * inNormal);
    vec3 normalView = normalize(mat3(view) * normalWorld);

    fragColor = inColor;
    fragNormalWorld = normalWorld;
    fragNormalView = normalView;

    vec4 worldPosition = model * vec4(inPosition, 1.0);
    vec4 viewPosition = view * worldPosition;
    fragViewPos = viewPosition.xyz;

    gl_Position = projection * viewPosition;
}
