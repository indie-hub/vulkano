#include <vulkano/mesh_gpu.hpp>

#include <span>
#include <stdexcept>
#include <string>

namespace vulkano {

void MeshGpuResources::upload(const VulkanContext& context, const MeshData& mesh, std::string_view name) {
    if(mesh.vertices.empty()) {
        throw std::runtime_error {"Mesh upload requires at least one vertex"};
    }
    if(mesh.indices.empty()) {
        throw std::runtime_error {"Mesh upload requires indices"};
    }

    const std::span<const MeshVertex> vertexSpan {mesh.vertices.data(), mesh.vertices.size()};
    const std::span<const std::uint32_t> indexSpan {mesh.indices.data(), mesh.indices.size()};

    m_vertexBuffer = Buffer::create_device_local_vertex_buffer(context, vertexSpan);
    m_indexBuffer = Buffer::create_device_local_index_buffer(context, indexSpan);
    m_vertexCount = static_cast<std::uint32_t>(mesh.vertices.size());
    m_indexCount = static_cast<std::uint32_t>(mesh.indices.size());

    const std::string vertexName = std::string {name} + " Vertex Buffer";
    const std::string indexName = std::string {name} + " Index Buffer";

    context.set_object_name(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<std::uint64_t>(m_vertexBuffer.handle()), vertexName);
    context.set_object_name(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<std::uint64_t>(m_indexBuffer.handle()), indexName);
}

auto MeshGpuResources::vertex_buffer() const noexcept -> VkBuffer {
    return m_vertexBuffer.handle();
}

auto MeshGpuResources::index_buffer() const noexcept -> VkBuffer {
    return m_indexBuffer.handle();
}

auto MeshGpuResources::index_count() const noexcept -> std::uint32_t {
    return m_indexCount;
}

auto MeshGpuResources::vertex_count() const noexcept -> std::uint32_t {
    return m_vertexCount;
}

auto MeshGpuResources::has_geometry() const noexcept -> bool {
    return m_vertexCount > 0U && m_indexCount > 0U;
}

} // namespace vulkano
