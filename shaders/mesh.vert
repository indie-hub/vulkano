#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vUv;
layout(location = 3) out vec3 vTangent;
layout(location = 4) out vec3 vBitangent;

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
    vec4 textureControls; // x: albedoEnabled, y: normalEnabled, z: normalStrength, w: unused
} primitiveConstants;

void main() {
    mat4 model = primitiveConstants.model;
    vec4 worldPosition = model * vec4(inPosition, 1.0);
    vWorldPos = worldPosition.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 normal = normalize(normalMatrix * inNormal);
    vec3 tangent = normalize(normalMatrix * inTangent.xyz);
    vec3 bitangent = normalize(cross(normal, tangent) * inTangent.w);
    vNormal = normal;
    vTangent = tangent;
    vBitangent = bitangent;
    vUv = inUv;

    gl_Position = globalUniforms.projection * globalUniforms.view * worldPosition;
}
