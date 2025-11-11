#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace vulkano::vk {
class ColorImage final {
public:
    ColorImage() noexcept = default;
    static ColorImage create(VkPhysicalDevice physicalDevice, VkDevice device, VkExtent2D extent, VkFormat format,
        VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    ColorImage(const ColorImage&) = delete;
    ColorImage& operator=(const ColorImage&) = delete;

    ColorImage(ColorImage&& other) noexcept;
    ColorImage& operator=(ColorImage&& other) noexcept;

    ~ColorImage() noexcept;

    [[nodiscard]] VkImage image() const noexcept;
    [[nodiscard]] VkImageView view() const noexcept;
    [[nodiscard]] VkFormat format() const noexcept;
    [[nodiscard]] VkExtent2D extent() const noexcept;

private:
    ColorImage(VkDevice device, VkImage image, VkDeviceMemory memory, VkImageView view, VkFormat format,
        VkExtent2D extent) noexcept;

    void destroy() noexcept;
    static std::uint32_t find_memory_type(
        VkPhysicalDevice physicalDevice, std::uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkDevice m_device {VK_NULL_HANDLE};
    VkImage m_image {VK_NULL_HANDLE};
    VkDeviceMemory m_memory {VK_NULL_HANDLE};
    VkImageView m_view {VK_NULL_HANDLE};
    VkFormat m_format {VK_FORMAT_UNDEFINED};
    VkExtent2D m_extent {0U, 0U};
};
} // namespace vulkano::vk
