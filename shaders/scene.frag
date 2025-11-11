#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragViewPos;

layout(set = 0, binding = 0) uniform SSAOConfig {
    float occlusionStrength;
    float baseAmbient;
    float debugView;
    float padding;
} uConfig;
layout(set = 0, binding = 1) uniform sampler2D ssaoTex;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outAlbedo;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out float outLinearDepth;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 albedo = fragColor;
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diffuse = max(dot(normal, lightDir), 0.0);

    vec2 occlusionUV = gl_FragCoord.xy / vec2(textureSize(ssaoTex, 0));
    float occlusion = texture(ssaoTex, occlusionUV).r;

    if (uConfig.debugView > 0.5) {
        vec3 debugColor = vec3(occlusion);
        outColor = vec4(debugColor, 1.0);
        outAlbedo = vec4(albedo, 1.0);
        outNormal = vec4(normal * 0.5 + 0.5, 1.0);
        outLinearDepth = -fragViewPos.z;
        return;
    }

    float occlusionFactor = mix(1.0, occlusion, clamp(uConfig.occlusionStrength, 0.0, 1.0));
    float ambient = uConfig.baseAmbient * occlusionFactor;

    vec3 lit = albedo * (diffuse + ambient);

    outColor = vec4(lit, 1.0);
    outAlbedo = vec4(albedo, 1.0);
    outNormal = vec4(normal * 0.5 + 0.5, 1.0);
    outLinearDepth = -fragViewPos.z;
}
