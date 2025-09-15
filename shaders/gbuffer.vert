#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 4) in vec2 inUV;

layout(push_constant) uniform PC {
    mat4 mvp;
} pc;

layout(set = 0, binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    vec4 camPos_normalStrength;
    vec4 lightPos_shininess;
    vec4 lightColor_intensity;
    vec4 screen_invW_invH_aoPower_radius;
} ubo;

layout(location = 0) out vec3 vNormalVS;
layout(location = 1) out float vDepthVS;
layout(location = 2) out vec2 vUV;

void main() {
    gl_Position = pc.mvp * vec4(inPos, 1.0);
    // view-space normal: ignore translation part
    mat3 view3 = mat3(ubo.view);
    vNormalVS = normalize(view3 * inNormal);
    vec3 posVS = (ubo.view * vec4(inPos, 1.0)).xyz;
    vDepthVS = -posVS.z; // positive distance from camera
    vUV = inUV;
}

