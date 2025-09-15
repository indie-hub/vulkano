#version 450

layout(location = 0) out float outAO;

layout(location = 0) in vec2 vUV;

layout(binding = 0) uniform sampler2D uAO;

layout(push_constant) uniform BlurPC {
    int radius;
    int horizontal;
} pc;

void main() {
    vec2 texel = 1.0 / vec2(textureSize(uAO, 0));
    float sum = 0.0;
    float weightSum = 0.0;
    for (int i = -pc.radius; i <= pc.radius; ++i) {
        float w = exp(-float(i * i) / 8.0);
        vec2 offset = pc.horizontal == 1 ? vec2(texel.x * float(i), 0.0) : vec2(0.0, texel.y * float(i));
        sum += texture(uAO, vUV + offset).r * w;
        weightSum += w;
    }
    outAO = sum / weightSum;
}

