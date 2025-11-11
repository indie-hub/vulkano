#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace vulkano::vk {
class DepthImage final {
public:
    DepthImage() noexcept = default;
    static DepthImage create(VkPhysicalDevice physicalDevice, VkDevice device, VkExtent2D extent, VkFormat format);

    DepthImage(const DepthImage&) = delete;
    DepthImage& operator=(const DepthImage&) = delete;

    DepthImage(DepthImage&& other) noexcept;
    DepthImage& operator=(DepthImage&& other) noexcept;

    ~DepthImage() noexcept;

    [[nodiscard]] VkImage image() const noexcept;
    [[nodiscard]] VkImageView view() const noexcept;
    [[nodiscard]] VkFormat format() const noexcept;
    [[nodiscard]] VkExtent2D extent() const noexcept;

private:
    DepthImage(VkDevice device, VkImage image, VkDeviceMemory memory, VkImageView view, VkFormat format,
        VkExtent2D extent) noexcept;

    void destroy() noexcept;
    static bool has_stencil_component(VkFormat format) noexcept;
    static std::uint32_t find_memory_type(VkPhysicalDevice physicalDevice, std::uint32_t typeFilter,
        VkMemoryPropertyFlags properties);

    VkDevice m_device {VK_NULL_HANDLE};
    VkImage m_image {VK_NULL_HANDLE};
    VkDeviceMemory m_memory {VK_NULL_HANDLE};
    VkImageView m_view {VK_NULL_HANDLE};
    VkFormat m_format {VK_FORMAT_UNDEFINED};
    VkExtent2D m_extent {0U, 0U};
};
} // namespace vulkano::vk
