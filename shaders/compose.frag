#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 vUV;

// Scene color is already lit in the G-buffer pass
layout(binding = 0) uniform sampler2D uScene;
layout(binding = 1) uniform sampler2D uAO;

void main() {
    vec3 scene = texture(uScene, vUV).rgb;
    float ao = texture(uAO, vUV).r;
    vec3 color = scene * ao;
    color = pow(color, vec3(1.0/2.2));
    outColor = vec4(color, 1.0);
}
