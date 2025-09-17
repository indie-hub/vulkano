#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out float outAO;

layout(std140, binding = 0) uniform BlurUBO {
    vec2 uInvExtent;
    int uRadius;
    float uSigma;
};

layout(binding = 1) uniform sampler2D uAO;

float gauss(float x, float s) {
    return exp(-0.5 * (x*x) / (s*s));
}

void main() {
    float sum = 0.0;
    float wsum = 0.0;
    for (int y = -uRadius; y <= uRadius; ++y) {
        for (int x = -uRadius; x <= uRadius; ++x) {
            vec2 o = vec2(float(x) * uInvExtent.x, float(y) * uInvExtent.y);
            float r = length(vec2(x, y));
            float w = gauss(r, uSigma);
            sum += texture(uAO, vUV + o).r * w;
            wsum += w;
        }
    }
    outAO = (wsum > 0.0) ? (sum / wsum) : texture(uAO, vUV).r;
}

