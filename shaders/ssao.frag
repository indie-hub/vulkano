#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out float outAO;

// SSAO set = 2 (matches C++ layout creation)
layout(set = 0, binding = 0) uniform sampler2D depthTex;
layout(set = 0, binding = 1) uniform sampler2D normalTex;
layout(set = 0, binding = 2) uniform sampler2D noiseTex;

layout(std140, set = 0, binding = 3) uniform SsaoParams {
    mat4 proj;
    mat4 invProj;
    vec2 noiseScale;
    float radius;
    float bias;
    float power;
    int kernelSize;
    vec4 kernel[64];
} U;

// Helpers
vec3 reconstructViewPos(vec2 uv, float depth)
{
    // Vulkan depth 0..1 -> NDC.z -1..1
    float z = depth * 2.0 - 1.0;
    vec4 ndc = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 view = U.invProj * ndc;
    view /= view.w;
    return view.xyz;
}

vec3 viewNormalFromDepth(vec2 uv)
{
    // Finite differences in view space
    vec2 texel = vec2(1.0 / U.noiseScale.x, 1.0 / U.noiseScale.y);
    float dC = texture(depthTex, uv).r;
    float dR = texture(depthTex, uv + vec2(texel.x, 0.0)).r;
    float dU = texture(depthTex, uv + vec2(0.0, texel.y)).r;
    vec3 pC = reconstructViewPos(uv, dC);
    vec3 pR = reconstructViewPos(uv + vec2(texel.x, 0.0), dR);
    vec3 pU = reconstructViewPos(uv + vec2(0.0, texel.y), dU);
    vec3 dx = pR - pC;
    vec3 dy = pU - pC;
    vec3 n = normalize(cross(dy, dx));
    return n;
}

void main()
{
    float depth = texture(depthTex, vUV).r;
    if (depth <= 0.00001)
    {
        outAO = 1.0;
        return;
    }
    vec3 posV = reconstructViewPos(vUV, depth);
    // Prefer provided normal if available; otherwise derive from depth
    vec3 nTex = texture(normalTex, vUV).xyz * 2.0 - 1.0;
    vec3 N = normalize(nTex);
    if (length(N) < 0.1)
    {
        N = viewNormalFromDepth(vUV);
    }

    // Noise-based tangent basis
    vec3 rand = texture(noiseTex, vUV * U.noiseScale).xyz * 2.0 - 1.0;
    vec3 T = normalize(rand - N * dot(rand, N));
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);

    float occlusion = 0.0;
    int ks = clamp(U.kernelSize, 1, 64);
    for (int i = 0; i < ks; ++i)
    {
        vec3 samp = TBN * U.kernel[i].xyz; // tangent to view
        samp = posV + samp * U.radius;
        // Project to NDC to sample depth buffer
        vec4 projPos = U.proj * vec4(samp, 1.0);
        projPos /= projPos.w;
        vec2 sampUV = projPos.xy * 0.5 + 0.5;
        if (sampUV.x < 0.0 || sampUV.x > 1.0 || sampUV.y < 0.0 || sampUV.y > 1.0)
        {
            continue;
        }
        float sampDepth = texture(depthTex, sampUV).r;
        vec3 sampPosV = reconstructViewPos(sampUV, sampDepth);
        float rangeCheck = smoothstep(0.0, 1.0, U.radius / abs(posV.z - sampPosV.z));
        float occluded = (sampPosV.z >= samp.z + U.bias) ? 1.0 : 0.0;
        occlusion += occluded * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / float(ks));
    outAO = pow(clamp(occlusion, 0.0, 1.0), max(U.power, 0.0001));
}
