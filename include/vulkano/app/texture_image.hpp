#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include <vulkano/app/texture_loader.hpp>

namespace vulkano::app {
class VulkanContext;

class TextureImage final {
public:
    TextureImage() noexcept = default;
    static TextureImage create(const VulkanContext& context, const TextureData& data);

    TextureImage(const TextureImage&) = delete;
    TextureImage& operator=(const TextureImage&) = delete;

    TextureImage(TextureImage&& other) noexcept;
    TextureImage& operator=(TextureImage&& other) noexcept;

    ~TextureImage() noexcept;

    [[nodiscard]] VkImage image() const noexcept;
    [[nodiscard]] VkImageView view() const noexcept;
    [[nodiscard]] VkSampler sampler() const noexcept;
    [[nodiscard]] VkFormat format() const noexcept;
    [[nodiscard]] VkExtent2D extent() const noexcept;

private:
    TextureImage(VkDevice device, VkImage image, VkDeviceMemory memory, VkImageView view, VkSampler sampler,
        VkFormat format, VkExtent2D extent) noexcept;

    void destroy() noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    VkImage m_image {VK_NULL_HANDLE};
    VkDeviceMemory m_memory {VK_NULL_HANDLE};
    VkImageView m_view {VK_NULL_HANDLE};
    VkSampler m_sampler {VK_NULL_HANDLE};
    VkFormat m_format {VK_FORMAT_UNDEFINED};
    VkExtent2D m_extent {0U, 0U};
};
} // namespace vulkano::app

