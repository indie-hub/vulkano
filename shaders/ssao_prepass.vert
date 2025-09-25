#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 vViewPos;
layout(location = 1) out vec3 vViewNormal;

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 projection;
    mat4 lightViewProjection[4];
    vec4 lightPositionIntensity;
    vec4 cameraPosition;
    vec4 shadowParams;
    vec4 shadowConfig;
    vec4 cascadeSplits;
    vec4 cameraClip;
} globalUniforms;

layout(push_constant) uniform PrimitivePushConstants {
    mat4 model;
    vec4 materialColor;
    vec4 materialProperties;
    vec4 textureControls;
} primitiveConstants;

void main() {
    mat4 model = primitiveConstants.model;
    vec4 worldPosition = model * vec4(inPosition, 1.0);

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 worldNormal = normalize(normalMatrix * inNormal);

    vec4 viewPosition = globalUniforms.view * worldPosition;
    mat3 viewMatrix3 = mat3(globalUniforms.view);
    vec3 viewNormal = normalize(viewMatrix3 * worldNormal);

    vViewPos = viewPosition.xyz;
    vViewNormal = viewNormal;

    gl_Position = globalUniforms.projection * viewPosition;
}
