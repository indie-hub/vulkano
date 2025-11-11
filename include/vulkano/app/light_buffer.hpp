#pragma once

#include <cstddef>

#include <vulkan/vulkan.h>

#include <vulkano/app/light_gpu.hpp>

namespace vulkano::app {
class VulkanContext;

class LightBuffer final {
public:
    explicit LightBuffer(const VulkanContext& context);
    ~LightBuffer() noexcept;

    LightBuffer(const LightBuffer&) = delete;
    LightBuffer& operator=(const LightBuffer&) = delete;
    LightBuffer(LightBuffer&&) noexcept = delete;
    LightBuffer& operator=(LightBuffer&&) noexcept = delete;

    void update(const scene::LightRegistry& registry);

    [[nodiscard]] VkBuffer buffer() const noexcept;
    [[nodiscard]] VkDescriptorBufferInfo descriptor_info() const noexcept;
    [[nodiscard]] std::size_t light_count() const noexcept;

private:
    void create_buffer(std::size_t lightCount);
    void destroy() noexcept;

    const VulkanContext& m_context;
    VkBuffer m_buffer {VK_NULL_HANDLE};
    VkDeviceMemory m_memory {VK_NULL_HANDLE};
    VkDeviceSize m_capacity {0U};
    std::size_t m_lightCount {0U};
    VkDescriptorBufferInfo m_descriptorInfo {};
};
}

