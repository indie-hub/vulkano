#include <vulkano/app/mesh_packing.hpp>

#include <vulkano/scene/mesh.hpp>

namespace vulkano::app {
std::vector<GpuVertex> pack_mesh_vertices(const scene::MeshData& mesh) noexcept {
    std::vector<GpuVertex> packed {};
    packed.reserve(mesh.vertices.size());
    for (const scene::Vertex& vertex : mesh.vertices) {
        packed.push_back(GpuVertex {
            .position = vertex.position,
            .normal = vertex.normal,
            .color = vertex.color,
            .uv = vertex.uv,
            .tangent = vertex.tangent,
            .bitangentSign = vertex.bitangentSign
        });
    }
    return packed;
}
} // namespace vulkano::app
