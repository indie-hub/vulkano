#version 450

layout(location = 0) out float outAO;

layout(location = 0) in vec2 vUV;

layout(binding = 0) uniform sampler2D uNormalTex; // view-space normal encoded in [0,1]
layout(binding = 1) uniform sampler2D uDepthTex;  // linear depth in [0,1]
layout(binding = 2) uniform sampler2D uNoiseTex;

layout(std140, binding = 3) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 cameraPos;
} uCamera;

layout(std140, binding = 4) uniform SsaoParams {
    int kernelSize;
    float radius;
    float bias;
    float power;
    int blurRadius;
    int enableBlur;
    int enabled;
    int pad;
} uSsao;

// Kernel samples as UBO for simplicity and portability
layout(std140, binding = 5) uniform KernelUBO {
    vec4 samples[64];
} uKernel;

void main() {
    if (uSsao.enabled == 0) {
        outAO = 1.0;
        return;
    }
    vec3 normal = normalize(texture(uNormalTex, vUV).xyz * 2.0 - 1.0);
    float depth = texture(uDepthTex, vUV).r;
    vec2 noiseScale = vec2(textureSize(uNoiseTex, 0));
    vec3 randomVec = normalize(texture(uNoiseTex, vUV * noiseScale).xyz * 2.0 - 1.0);
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < uSsao.kernelSize; ++i) {
        vec3 sampleVec = uKernel.samples[i].xyz;
        vec3 samplePos = TBN * sampleVec; // oriented around normal
        samplePos = samplePos * uSsao.radius + vec3(vUV, depth);
        float sampleDepth = texture(uDepthTex, samplePos.xy).r;
        float rangeCheck = smoothstep(0.0, 1.0, uSsao.radius / abs(depth - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + uSsao.bias ? 1.0 : 0.0) * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / float(uSsao.kernelSize));
    outAO = pow(occlusion, uSsao.power);
}
