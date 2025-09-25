#pragma once

#include <cstdint>
#include <memory>
#include <span>

#include <vulkano/vulkan_context.hpp>

#include <vulkan/vulkan.h>

namespace vulkano {

class Texture2D final {
public:
    Texture2D() = default;
    Texture2D(const Texture2D&) = delete;
    Texture2D(Texture2D&& other) noexcept;
    auto operator=(const Texture2D&) -> Texture2D& = delete;
    auto operator=(Texture2D&& other) noexcept -> Texture2D&;
    ~Texture2D() noexcept;

    [[nodiscard]] static auto create_rgba8(
        const VulkanContext& context,
        VkExtent2D extent,
        std::span<const std::uint8_t> pixels,
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
        VkFilter filter = VK_FILTER_LINEAR) -> Texture2D;

    [[nodiscard]] auto image() const noexcept -> VkImage;
    [[nodiscard]] auto image_view() const noexcept -> VkImageView;
    [[nodiscard]] auto sampler() const noexcept -> VkSampler;
    [[nodiscard]] auto extent() const noexcept -> VkExtent2D;
    [[nodiscard]] auto format() const noexcept -> VkFormat;

private:
    Texture2D(
        const VulkanContext& context,
        VkExtent2D extent,
        VkFormat format,
        std::span<const std::uint8_t> pixels,
        VkFilter filter);

    void initialise_image(const VulkanContext& context, VkExtent2D extent, VkFormat format);
    void allocate_memory(const VulkanContext& context);
    void create_image_view(const VulkanContext& context);
    void create_sampler(const VulkanContext& context, VkFilter filter);
    void upload_pixels(const VulkanContext& context, std::span<const std::uint8_t> pixels);
    void destroy() noexcept;
    void move_from(Texture2D&& other) noexcept;

    static auto find_memory_type(
        const VulkanContext& context,
        std::uint32_t typeFilter,
        VkMemoryPropertyFlags properties) -> std::uint32_t;

    VkDevice m_device {VK_NULL_HANDLE};
    VkImage m_image {VK_NULL_HANDLE};
    VkDeviceMemory m_memory {VK_NULL_HANDLE};
    VkImageView m_imageView {VK_NULL_HANDLE};
    VkSampler m_sampler {VK_NULL_HANDLE};
    VkExtent2D m_extent {0U, 0U};
    VkFormat m_format {VK_FORMAT_UNDEFINED};
};

using TextureHandle = std::shared_ptr<Texture2D>;

} // namespace vulkano

