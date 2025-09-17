#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 vViewNormal;

layout(std140, binding = 0) uniform GlobalUBO {
    mat4 uView;
    mat4 uProj;
    vec3 uCameraPos;
    float uAmbientStrength;
    vec3 uLightPos;
    float uLightIntensity;
    vec3 uLightColor;
    float _pad0;
};

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec3 baseColor; // unused in this shader
    float shininess; // unused
    int useAlbedoMap; // unused
    int useNormalMap; // unused
    float normalStrength; // unused
    float _pad1;
} pc;

void main() {
    mat3 normalMat = mat3(transpose(inverse(pc.model)));
    vec3 nWorld = normalize(normalMat * inNormal);
    // View space normal
    mat3 view3 = mat3(uView);
    vViewNormal = normalize(view3 * nWorld);
    gl_Position = uProj * uView * pc.model * vec4(inPos, 1.0);
}

