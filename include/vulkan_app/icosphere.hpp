#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace vulkan_app {

/**
 * @brief Vertex for sphere rendering with tangent-space basis.
 *
 * Contains position, normal, UV, and TBN vectors used for normal mapping.
 */
struct Vertex final {
    glm::vec3 position{};
    glm::vec3 normal{};
    glm::vec2 uv{};
    glm::vec3 tangent{};
    glm::vec3 bitangent{};
};

/**
 * @brief Indexed mesh container.
 */
struct Mesh final {
    std::vector<Vertex> vertices{};
    std::vector<std::uint32_t> indices{};
};

/**
 * @brief Generate an icosphere mesh with the given subdivision level.
 *
 * Starts from a unit icosahedron and subdivides each triangle; all vertices are
 * normalized to the unit sphere after each step. UVs and a reasonable TBN are
 * generated per vertex for normal mapping.
 *
 * @param subdivisions Number of refinement steps, recommended [0..6].
 * @return Mesh containing vertices and indices.
 */
[[nodiscard]] Mesh make_icosphere(int subdivisions);

} // namespace vulkan_app
