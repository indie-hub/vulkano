#version 450

const uint MAX_SHADOW_CASCADES = 4u;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

struct ShadowUniform {
    mat4 lightViewProjection[MAX_SHADOW_CASCADES];
    vec4 cascadeData[MAX_SHADOW_CASCADES];
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

layout(push_constant) uniform ShadowConstants {
    mat4 model;
    uint cascadeIndex;
    vec3 padding;
} shadowConstants;

void main() {
    uint cascade = min(shadowConstants.cascadeIndex, MAX_SHADOW_CASCADES - 1u);
    vec4 worldPosition = shadowConstants.model * vec4(inPosition, 1.0);
    mat4 lightViewProjection = globalUniforms.shadow.lightViewProjection[cascade];
    gl_Position = lightViewProjection * worldPosition;
}
