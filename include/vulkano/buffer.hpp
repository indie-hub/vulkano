#pragma once

#include <vulkano/mesh.hpp>
#include <vulkano/vulkan_context.hpp>

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

    [[nodiscard]] static auto create_vertex_buffer(const VulkanContext& context, std::span<const MeshVertex> vertices) -> Buffer;
    [[nodiscard]] static auto create_device_local_vertex_buffer(const VulkanContext& context, std::span<const MeshVertex> vertices) -> Buffer;
    [[nodiscard]] static auto create_device_local_index_buffer(const VulkanContext& context, std::span<const std::uint32_t> indices) -> Buffer;
    [[nodiscard]] static auto create_uniform_buffer(const VulkanContext& context, VkDeviceSize size) -> Buffer;

    [[nodiscard]] auto handle() const noexcept -> VkBuffer;
    [[nodiscard]] auto size() const noexcept -> VkDeviceSize;
    void write(const VulkanContext& context, const void* data, VkDeviceSize size, VkDeviceSize offset = 0U);

private:
    Buffer(const VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

    void initialise(const VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void copy_data(const VulkanContext& context, const void* data, VkDeviceSize size);
    void cleanup() noexcept;
    void move_from(Buffer&& other) noexcept;
    [[nodiscard]] auto find_memory_type(const VulkanContext& context, std::uint32_t typeFilter, VkMemoryPropertyFlags properties) -> std::uint32_t;
    static void copy_buffer(const VulkanContext& context, VkBuffer source, VkBuffer destination, VkDeviceSize size);

    VkDevice m_device {VK_NULL_HANDLE};
    VkBuffer m_buffer {VK_NULL_HANDLE};
    VkDeviceMemory m_memory {VK_NULL_HANDLE};
    VkDeviceSize m_size {0U};
};

} // namespace vulkano
