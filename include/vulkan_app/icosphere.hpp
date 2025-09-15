#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace vulkan_app {

struct Vertex final {
    glm::vec3 position{};
    glm::vec3 normal{};
    glm::vec2 uv{};
    glm::vec3 tangent{};
    glm::vec3 bitangent{};
};

struct Mesh final {
    std::vector<Vertex> vertices{};
    std::vector<std::uint32_t> indices{};
};

[[nodiscard]] Mesh make_icosphere(int subdivisions);

} // namespace vulkan_app
