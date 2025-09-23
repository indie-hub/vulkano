#version 450

const uint MAX_SHADOW_CASCADES = 4u;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vUv;
layout(location = 3) out vec3 vTangent;
layout(location = 4) out vec3 vBitangent;

struct ShadowUniform {
    mat4 lightViewProjection[MAX_SHADOW_CASCADES];
    vec4 cascadeSplits;
    vec4 shadowParams;
    vec4 biasParams;
    vec4 atlasSize;
    vec4 debugParams;
};

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 view;
    mat4 projection;
    vec4 lightDirectionIntensity;
    vec4 cameraPosition;
    ShadowUniform shadow;
} globalUniforms;

layout(push_constant) uniform PrimitivePushConstants {
    mat4 model;
    vec4 materialColor;
    vec4 materialProperties;
} primitiveConstants;

void main() {
    mat4 model = primitiveConstants.model;
    vec4 worldPosition = model * vec4(inPosition, 1.0);
    vWorldPos = worldPosition.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 worldNormal = normalize(normalMatrix * inNormal);
    vec3 worldTangent = normalize(normalMatrix * inTangent.xyz);
    vec3 worldBitangent = normalize(cross(worldNormal, worldTangent) * inTangent.w);

    vNormal = worldNormal;
    vTangent = worldTangent;
    vBitangent = worldBitangent;
    vUv = inUv;

    gl_Position = globalUniforms.projection * globalUniforms.view * worldPosition;
}
