#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out float outAO;

layout(set = 0, binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    vec4 camPos_normalStrength;
    vec4 lightPos_shininess;
    vec4 lightColor_intensity;
    // x=invWidth, y=invHeight, z=aoPower, w=radius
    vec4 screen_invW_invH_aoPower_radius;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D uND; // gbuffer normal/depth packed

// Simple 2D SSAO kernel
const int KERNEL = 16;
const vec2 OFFSETS[KERNEL] = vec2[KERNEL](
    vec2( 1,  0), vec2(-1,  0), vec2( 0,  1), vec2( 0, -1),
    vec2( 1,  1), vec2(-1,  1), vec2( 1, -1), vec2(-1, -1),
    vec2( 2,  0), vec2(-2,  0), vec2( 0,  2), vec2( 0, -2),
    vec2( 2,  1), vec2(-2,  1), vec2( 1,  2), vec2(-1, -2)
);

void main() {
    vec2 texel = vec2(ubo.screen_invW_invH_aoPower_radius.x, ubo.screen_invW_invH_aoPower_radius.y);
    float radius = max(ubo.screen_invW_invH_aoPower_radius.w, 1.0);

    vec4 nd = texture(uND, vUV);
    vec3 n = normalize(nd.xyz * 2.0 - 1.0);
    float depth = nd.w;

    float occlusion = 0.0;
    for (int i = 0; i < KERNEL; ++i) {
        vec2 offset = OFFSETS[i] * texel * radius;
        vec4 ndS = texture(uND, vUV + offset);
        float depthS = ndS.w;
        float range = smoothstep(0.0, 1.0, radius);
        float delta = depthS - depth;
        float occ = step(0.001, delta) * (1.0 - clamp(delta / (radius), 0.0, 1.0)) * range;
        occlusion += occ;
    }
    occlusion /= float(KERNEL);
    outAO = 1.0 - occlusion;
}

