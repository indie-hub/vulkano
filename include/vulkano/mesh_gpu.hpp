#pragma once

#include <cstdint>
#include <string_view>

#include <vulkano/buffer.hpp>
#include <vulkano/mesh.hpp>

namespace vulkano {

class MeshGpuResources final {
public:
    MeshGpuResources() = default;

    void upload(const VulkanContext& context, const MeshData& mesh, std::string_view name);

    [[nodiscard]] auto vertex_buffer() const noexcept -> VkBuffer;
    [[nodiscard]] auto index_buffer() const noexcept -> VkBuffer;
    [[nodiscard]] auto index_count() const noexcept -> std::uint32_t;
    [[nodiscard]] auto vertex_count() const noexcept -> std::uint32_t;
    [[nodiscard]] auto has_geometry() const noexcept -> bool;

private:
    Buffer m_vertexBuffer {};
    Buffer m_indexBuffer {};
    std::uint32_t m_indexCount {0U};
    std::uint32_t m_vertexCount {0U};
};

} // namespace vulkano

