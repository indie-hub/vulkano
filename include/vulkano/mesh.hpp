#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace vulkano {

struct MeshVertex final {
    glm::vec3 position {};
    glm::vec3 normal {};
    glm::vec2 uv {};
    glm::vec4 tangent {0.0F, 0.0F, 0.0F, 1.0F};
};

struct MeshData final {
    std::vector<MeshVertex> vertices {};
    std::vector<std::uint32_t> indices {};
};

struct PrimitiveProperties final {
    glm::vec3 position {0.0F, 0.0F, 0.0F};
    glm::vec3 rotation {0.0F, 0.0F, 0.0F};
    glm::vec3 scale {1.0F, 1.0F, 1.0F};
    glm::vec3 baseColor {1.0F, 1.0F, 1.0F};
    float shininess {32.0F};
    float ambientStrength {0.1F};
    float specularStrength {0.5F};
    bool useAlbedoMap {true};
    bool useNormalMap {true};
    float normalStrength {1.0F};
    glm::vec2 uvScale {1.0F, 1.0F};
    std::string albedoTexturePath {};
    std::string normalTexturePath {};
};

struct PlaneParameters final {
    float width {1.0F};
    float depth {1.0F};
    glm::vec2 uvTiling {1.0F, 1.0F};
};

struct IcosphereParameters final {
    std::uint32_t subdivisions {0U};
};

} // namespace vulkano
