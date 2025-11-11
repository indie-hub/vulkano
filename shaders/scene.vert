#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in float inBitangentSign;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormalWorld;
layout(location = 2) out vec3 fragViewPos;
layout(location = 3) out vec3 fragNormalView;
layout(location = 4) out vec2 fragUV;
layout(location = 5) out vec3 fragPosWorld;
layout(location = 6) flat out uint fragMaterialIndex;
layout(location = 7) out vec3 fragTangentWorld;
layout(location = 8) flat out float fragBitangentSign;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 projection;
    uvec4 materialData;
    vec4 cameraPosition;
} pushConstants;

void main() {
    mat4 model = pushConstants.model;
    mat4 view = pushConstants.view;
    mat4 projection = pushConstants.projection;
    mat3 normalMatrix = mat3(transpose(inverse(model)));

    vec3 normalWorld = normalize(normalMatrix * inNormal);
    vec3 normalView = normalize(mat3(view) * normalWorld);
    vec3 tangentWorld = normalize(normalMatrix * inTangent);

    fragColor = inColor;
    fragNormalWorld = normalWorld;
    fragNormalView = normalView;
    fragUV = inUV;
    fragTangentWorld = tangentWorld;
    fragBitangentSign = inBitangentSign;

    vec4 worldPosition = model * vec4(inPosition, 1.0);
    vec4 viewPosition = view * worldPosition;
    fragViewPos = viewPosition.xyz;
    fragPosWorld = worldPosition.xyz;

    fragMaterialIndex = pushConstants.materialData.x;
    gl_Position = projection * viewPosition;
}
