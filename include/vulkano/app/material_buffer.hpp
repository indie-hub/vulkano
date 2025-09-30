#pragma once

#include <cstddef>
#include <cstdint>

#include <vulkan/vulkan.h>

#include <vulkano/app/material_gpu.hpp>

namespace vulkano::app {
class VulkanContext;

class MaterialBuffer final {
public:
    explicit MaterialBuffer(const VulkanContext& context);
    ~MaterialBuffer() noexcept;

    MaterialBuffer(const MaterialBuffer&) = delete;
    MaterialBuffer& operator=(const MaterialBuffer&) = delete;
    MaterialBuffer(MaterialBuffer&&) noexcept = delete;
    MaterialBuffer& operator=(MaterialBuffer&&) noexcept = delete;

    void update(const scene::MaterialRegistry& registry, const std::vector<scene::MaterialTextureHandles>& handles);

    [[nodiscard]] VkBuffer buffer() const noexcept;
    [[nodiscard]] VkDescriptorBufferInfo descriptor_info() const noexcept;
    [[nodiscard]] std::size_t material_count() const noexcept;

private:
    void create_buffer(std::size_t materialCount);
    void destroy() noexcept;

    const VulkanContext& m_context;
    VkBuffer m_buffer {VK_NULL_HANDLE};
    VkDeviceMemory m_memory {VK_NULL_HANDLE};
    VkDeviceSize m_capacity {0U};
    std::size_t m_materialCount {0U};
    VkDescriptorBufferInfo m_descriptorInfo {};
};
}
