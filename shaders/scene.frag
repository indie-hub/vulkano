#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outAlbedo;
layout(location = 2) out vec4 outNormal;

void main() {
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    vec3 normal = normalize(fragNormal);
    float diffuse = max(dot(normal, lightDir), 0.15);
    vec3 albedo = fragColor;
    outColor = vec4(albedo * diffuse, 1.0);
    outAlbedo = vec4(albedo, 1.0);
    outNormal = vec4(normal * 0.5 + 0.5, 1.0);
}
