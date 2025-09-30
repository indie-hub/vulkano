#version 450

layout(location = 0) in vec2 fragUV;

layout(set = 0, binding = 0) uniform sampler2D occlusionTex;
layout(set = 0, binding = 1) uniform sampler2D depthTex;
layout(set = 0, binding = 2) uniform BlurParams {
    float radius;
    float depthSigma;
    vec2 padding;
} uParams;

layout(location = 0) out float outOcclusion;

float gaussian(float x, float sigma) {
    float denom = max(sigma, 1e-4);
    return exp(-(x * x) / (2.0 * denom * denom));
}

void main() {
    vec2 textureSizeVec = vec2(textureSize(occlusionTex, 0));
    if (textureSizeVec.x <= 0.0 || textureSizeVec.y <= 0.0) {
        outOcclusion = texture(occlusionTex, fragUV).r;
        return;
    }

    vec2 texel = 1.0 / textureSizeVec;
    float sampleRadius = clamp(uParams.radius, 1.0, 6.0);
    int radiusSteps = int(ceil(sampleRadius));

    float centerOcclusion = texture(occlusionTex, fragUV).r;
    float centerDepth = texture(depthTex, fragUV).r;

    float weightSum = 0.0;
    float occlusionSum = 0.0;

    for (int y = -radiusSteps; y <= radiusSteps; ++y) {
        for (int x = -radiusSteps; x <= radiusSteps; ++x) {
            vec2 offset = vec2(x, y) * texel;
            vec2 sampleUV = fragUV + offset;
            if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) {
                continue;
            }

            float sampleOcc = texture(occlusionTex, sampleUV).r;
            float sampleDepth = texture(depthTex, sampleUV).r;

            float spatialWeight = gaussian(length(vec2(x, y)), sampleRadius);
            float depthDifference = sampleDepth - centerDepth;
            float depthWeight = gaussian(depthDifference, uParams.depthSigma);
            float weight = spatialWeight * depthWeight;

            occlusionSum += sampleOcc * weight;
            weightSum += weight;
        }
    }

    if (weightSum <= 0.0) {
        outOcclusion = centerOcclusion;
    } else {
        outOcclusion = clamp(occlusionSum / weightSum, 0.0, 1.0);
    }
}
