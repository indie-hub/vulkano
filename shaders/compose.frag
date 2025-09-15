#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 vUV;

layout(binding = 0) uniform sampler2D uAlbedo;
layout(binding = 1) uniform sampler2D uNormal;
layout(binding = 2) uniform sampler2D uDepth;
layout(binding = 3) uniform sampler2D uAO;

layout(std140, binding = 4) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 cameraPos;
} uCamera;

layout(std140, binding = 5) uniform LightUBO {
    vec3 lightPos;
    float intensity;
    vec3 lightColor;
    float shininess;
} uLight;

void main() {
    vec3 albedo = texture(uAlbedo, vUV).rgb;
    vec3 normal = normalize(texture(uNormal, vUV).xyz * 2.0 - 1.0);
    float ao = texture(uAO, vUV).r;

    vec3 fragPos = vec3(0.0); // reconstruct from depth if needed (omitted here for brevity)

    vec3 L = normalize(uLight.lightPos - fragPos);
    vec3 V = normalize(uCamera.cameraPos - fragPos);
    vec3 H = normalize(L + V);
    float NdotL = max(dot(normal, L), 0.0);
    float diffuse = NdotL;
    float spec = pow(max(dot(normal, H), 0.0), uLight.shininess);
    vec3 color = (albedo * diffuse + spec * uLight.lightColor * uLight.intensity) * ao;

    // Gamma correction
    color = pow(color, vec3(1.0/2.2));
    outColor = vec4(color, 1.0);
}

