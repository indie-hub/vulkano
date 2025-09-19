#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

// Compose set = 4
layout(set = 0, binding = 0) uniform sampler2D litTex;
layout(set = 0, binding = 1) uniform sampler2D aoTex;

layout(std140, set = 0, binding = 2) uniform ComposeParams {
    float aoStrength;
    float gamma;
    float exposure;
    float _pad;
} U;

void main()
{
    vec3 lit = texture(litTex, vUV).rgb;
    float ao = texture(aoTex, vUV).r;
    float aoMix = mix(1.0, ao, U.aoStrength);
    vec3 color = lit * aoMix;
    // Simple gamma
    color = pow(max(color, vec3(0.0)), vec3(1.0 / max(U.gamma, 0.0001)));
    outColor = vec4(color, 1.0);
}
