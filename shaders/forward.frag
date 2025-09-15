#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vTangent;
layout(location = 2) in vec3 vBitangent;
layout(location = 3) in vec2 vUV;
layout(location = 4) in vec3 vPosition;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    float normalStrength;
    vec3 lightPos;
    float shininess;
    vec3 lightColor;
    float lightIntensity;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D uAlbedo;
layout(set = 0, binding = 2) uniform sampler2D uNormal;

vec3 applyNormalMap(vec3 n, vec3 t, vec3 b, vec2 uv) {
    // Sample normal map (assumed in [0,1] RGB with (0,0,1) as flat)
    vec3 map = texture(uNormal, uv).xyz * 2.0 - 1.0;
    map = normalize(mix(vec3(0.0, 0.0, 1.0), map, clamp(ubo.normalStrength, 0.0, 2.0)));
    mat3 TBN = mat3(normalize(t), normalize(b), normalize(n));
    vec3 worldN = normalize(TBN * map);
    return worldN;
}

void main() {
    vec3 n = normalize(vNormal);
    vec3 t = normalize(vTangent);
    vec3 b = normalize(vBitangent);
    vec3 N = applyNormalMap(n, t, b, vUV);

    vec3 albedo = texture(uAlbedo, vUV).rgb;

    // Simple Phong lighting in world space with camera at ubo.cameraPos
    vec3 L = normalize(ubo.lightPos - vPosition);
    vec3 V = normalize(ubo.cameraPos - vPosition);
    vec3 H = normalize(L + V);
    float NdotL = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), max(ubo.shininess, 1.0));

    vec3 diffuse = albedo * NdotL;
    vec3 specular = ubo.lightColor * spec;
    vec3 color = (diffuse + specular) * ubo.lightColor * ubo.lightIntensity;

    // Gamma correct to sRGB for output
    color = pow(color, vec3(1.0/2.2));
    outColor = vec4(color, 1.0);
}

