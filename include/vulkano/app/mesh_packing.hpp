#pragma once

#include <vector>

#include <vulkano/app/gpu_vertex.hpp>
#include <vulkano/scene/mesh.hpp>

namespace vulkano::app {
[[nodiscard]] std::vector<GpuVertex> pack_mesh_vertices(const scene::MeshData& mesh) noexcept;
}
