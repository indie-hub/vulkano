#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec3 vWorldPos;
layout(location = 3) in vec3 vTangent;
layout(location = 4) in vec3 vBitangent;
layout(location = 5) in vec2 vScreenUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    float ambientStrength;
    vec3 lightPos;
    float lightIntensity;
    vec3 lightColor;
    float aoStrength;
    float ssaoEnabled;
    float _pad0;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec3 baseColor;
    float shininess;
    int useAlbedoMap;
    int useNormalMap;
    float normalStrength;
    float _pad1;
} PC;

layout(set = 0, binding = 1) uniform sampler2D albedoMap;
layout(set = 0, binding = 2) uniform sampler2D normalMap;
layout(set = 0, binding = 3) uniform sampler2D aoTex;

void main()
{
    // Build TBN from per-vertex attributes
    vec3 N = normalize(vNormal);
    vec3 T = normalize(vTangent);
    vec3 B = normalize(vBitangent);

    // Sample normal map (tangent-space) if enabled
    if (PC.useNormalMap != 0) {
        vec3 n = texture(normalMap, vUV).xyz * 2.0 - 1.0;
        n.xy *= PC.normalStrength;
        n.z = sqrt(max(0.0, 1.0 - dot(n.xy, n.xy)));
        // Transform to world space
        mat3 TBN = mat3(T, B, N);
        N = normalize(TBN * n);
    }

    // Albedo
    vec3 albedo = PC.baseColor;
    if (PC.useAlbedoMap != 0) {
        albedo *= texture(albedoMap, vUV).rgb;
    }

    // Blinn-Phong shading
    vec3 L = normalize(ubo.lightPos - vWorldPos);
    vec3 V = normalize(ubo.cameraPos - vWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = 0.0;
    if (diff > 0.0) {
        spec = pow(max(dot(N, H), 0.0), max(PC.shininess, 1.0));
    }

    vec3 ambient = ubo.ambientStrength * albedo;
    vec3 diffuse = diff * albedo * ubo.lightColor * ubo.lightIntensity;
    vec3 specular = spec * ubo.lightColor * ubo.lightIntensity;
    vec3 color = ambient + diffuse + specular;
    // Apply SSAO as post-lighting multiplier
    float aoVal = texture(aoTex, vScreenUV).r;
    float strength = (ubo.ssaoEnabled > 0.5) ? ubo.aoStrength : 0.0;
    float aoFactor = mix(1.0, aoVal, clamp(strength, 0.0, 1.5));
    color *= aoFactor;
    outColor = vec4(color, 1.0);
}
