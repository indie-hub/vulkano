#version 450

layout(set = 0, binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    vec4 cam_pos;    // w = normal strength
    vec4 light_pos;  // w = shininess
    vec4 light_col;  // w = intensity
    vec4 screen;     // x = invWidth, y = invHeight, z = ssao_power, w = radius or blur_radius
} ubo;

layout(set = 0, binding = 1) uniform sampler2D uAO;

layout(location = 0) out float outAO;

// Simple isotropic Gaussian blur. Uses ubo.screen.w as integer radius (1..5).
void main() {
    ivec2 texSize = textureSize(uAO, 0);
    vec2 uv = gl_FragCoord.xy * vec2(ubo.screen.x, ubo.screen.y);

    int radius = int(clamp(ubo.screen.w, 1.0, 5.0));
    float sigma = max(1.0, float(radius) * 0.5);
    float twoSigma2 = 2.0 * sigma * sigma;

    float sum = 0.0;
    float wsum = 0.0;
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            vec2 offset = vec2(float(x) * ubo.screen.x, float(y) * ubo.screen.y);
            float d2 = float(x*x + y*y);
            float w = exp(-d2 / twoSigma2);
            sum += texture(uAO, uv + offset).r * w;
            wsum += w;
        }
    }
    float ao = sum / max(wsum, 1e-4);
    outAO = ao;
}

