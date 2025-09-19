#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 4, binding = 0) uniform sampler2D litColor;
layout(set = 4, binding = 1) uniform sampler2D aoTex;

layout(std140, set = 4, binding = 2) uniform ComposeParams {
    float aoStrength;
    float gamma;
    float exposure; // reserved for HDR tonemap
} C;

void main()
{
    vec3 color = texture(litColor, vUV).rgb;
    float ao = texture(aoTex, vUV).r;
    // Current placeholder: no AO applied yet to keep visuals unchanged
    vec3 outCol = color;
    // Optional gamma correction
    float g = max(C.gamma, 0.001);
    outCol = pow(outCol, vec3(1.0 / g));
    outColor = vec4(outCol, 1.0);
}

