#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out float outAO;

layout(std140, binding = 0) uniform SsaoUBO {
    mat4 uProj;
    mat4 uInvProj;
    vec2 uNoiseScale;
    float uRadius;
    float uBias;
    float uPower;
    int uKernelSize;
    vec4 uKernel[64];
};

layout(binding = 1) uniform sampler2D uDepthTex;
layout(binding = 2) uniform sampler2D uNormalTex;
layout(binding = 3) uniform sampler2D uNoiseTex;

float linearizeDepth(float depth) {
    // depth is [0,1] in clip space; reconstruct view-space z via inverse projection
    // Build NDC
    float z = depth * 2.0 - 1.0;
    vec4 clip = vec4(vUV * 2.0 - 1.0, z, 1.0);
    vec4 view = uInvProj * clip;
    view /= view.w;
    return -view.z; // right-handed view z negative forward
}

vec3 reconstructViewPos(float depth) {
    float z = depth * 2.0 - 1.0;
    vec4 clip = vec4(vUV * 2.0 - 1.0, z, 1.0);
    vec4 view = uInvProj * clip;
    view /= view.w;
    return view.xyz;
}

void main() {
    // Fetch normal (packed) and depth
    vec3 N = texture(uNormalTex, vUV).xyz * 2.0 - 1.0;
    N = normalize(N);
    float depth = texture(uDepthTex, vUV).r;
    vec3 pos = reconstructViewPos(depth);

    // Noise-based tangent space
    vec3 rand = texture(uNoiseTex, vUV * uNoiseScale).xyz * 2.0 - 1.0;
    vec3 T = normalize(rand - N * dot(rand, N));
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);

    float occlusion = 0.0;
    for (int i = 0; i < uKernelSize; ++i) {
        vec3 samp = TBN * uKernel[i].xyz; // already scaled within kernel build
        vec3 sp = pos + samp * uRadius;
        // Project sample to screen
        vec4 spClip = uProj * vec4(sp, 1.0);
        spClip /= spClip.w;
        vec2 suv = spClip.xy * 0.5 + 0.5;
        // Depth at sample
        float sd = texture(uDepthTex, suv).r;
        float svz = linearizeDepth(sd);
        float vz = -sp.z; // view-space z positive as distance
        float rangeCheck = smoothstep(0.0, 1.0, uRadius / abs(vz - svz));
        float diff = (svz >= vz + uBias) ? 1.0 : 0.0;
        occlusion += diff * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / float(uKernelSize));
    occlusion = pow(clamp(occlusion, 0.0, 1.0), uPower);
    outAO = occlusion;
}

