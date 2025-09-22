#pragma once

#include <vulkano/vulkan_context.hpp>
#include <vulkano/vertex.hpp>

#include <span>
#include <vulkan/vulkan.h>

namespace vulkano {

class Buffer final {
public:
    Buffer() = default;
    Buffer(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept;
    auto operator=(const Buffer&) -> Buffer& = delete;
    auto operator=(Buffer&& other) noexcept -> Buffer&;
    ~Buffer() noexcept;

    [[nodiscard]] static auto create_vertex_buffer(const VulkanContext& context, std::span<const Vertex> vertices) -> Buffer;

    [[nodiscard]] auto handle() const noexcept -> VkBuffer;
    [[nodiscard]] auto size() const noexcept -> VkDeviceSize;

private:
    Buffer(const VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

    void initialise(const VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void copy_data(const VulkanContext& context, const void* data, VkDeviceSize size);
    void cleanup() noexcept;
    void move_from(Buffer&& other) noexcept;
    [[nodiscard]] auto find_memory_type(const VulkanContext& context, std::uint32_t typeFilter, VkMemoryPropertyFlags properties) -> std::uint32_t;

    VkDevice m_device {VK_NULL_HANDLE};
    VkBuffer m_buffer {VK_NULL_HANDLE};
    VkDeviceMemory m_memory {VK_NULL_HANDLE};
    VkDeviceSize m_size {0U};
};

} // namespace vulkano
