#pragma once

#include <cstdint>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.h>

#include <vulkano/vulkan_context.hpp>

namespace vulkano {

class TextureImage final {
public:
    TextureImage() = default;
    TextureImage(const TextureImage&) = delete;
    TextureImage(TextureImage&& other) noexcept;
    auto operator=(const TextureImage&) -> TextureImage& = delete;
    auto operator=(TextureImage&& other) noexcept -> TextureImage&;
    ~TextureImage() noexcept;

    [[nodiscard]] static auto create(
        const VulkanContext& context,
        VkExtent2D extent,
        VkFormat format,
        VkImageUsageFlags usage,
        VkImageAspectFlags aspectMask,
        std::uint32_t mipLevels,
        std::string_view name) -> TextureImage;

    [[nodiscard]] static auto create_from_rgba(
        const VulkanContext& context,
        VkExtent2D extent,
        std::span<const std::uint8_t> pixels,
        VkFormat format,
        std::string_view name) -> TextureImage;

    [[nodiscard]] static auto calculate_mip_levels(VkExtent2D extent) -> std::uint32_t;

    void transition_layout(
        const VulkanContext& context,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        std::uint32_t baseMipLevel,
        std::uint32_t mipLevelCount = 1U) const;

    void copy_buffer_to_image(
        const VulkanContext& context,
        VkBuffer buffer,
        VkExtent3D extent,
        std::uint32_t baseMipLevel = 0U) const;

    void generate_mipmaps(const VulkanContext& context) const;

    [[nodiscard]] auto image() const noexcept -> VkImage;
    [[nodiscard]] auto image_view() const noexcept -> VkImageView;
    [[nodiscard]] auto mip_levels() const noexcept -> std::uint32_t;
    [[nodiscard]] auto extent() const noexcept -> VkExtent2D;
    [[nodiscard]] auto format() const noexcept -> VkFormat;

private:
    TextureImage(
        const VulkanContext& context,
        VkExtent2D extent,
        VkFormat format,
        VkImageUsageFlags usage,
        VkImageAspectFlags aspectMask,
        std::uint32_t mipLevels,
        std::string_view name);

    void cleanup() noexcept;
    void move_from(TextureImage&& other) noexcept;

    static void create_image(
        const VulkanContext& context,
        VkExtent2D extent,
        VkFormat format,
        VkImageUsageFlags usage,
        std::uint32_t mipLevels,
        VkImage& image,
        VkDeviceMemory& memory);

    static auto find_memory_type(
        const VulkanContext& context,
        std::uint32_t typeFilter,
        VkMemoryPropertyFlags properties) -> std::uint32_t;

    static void create_image_view(
        const VulkanContext& context,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspectMask,
        std::uint32_t mipLevels,
        VkImageView& imageView);

    static auto begin_single_use_commands(const VulkanContext& context, VkCommandPool& commandPool) -> VkCommandBuffer;
    static void end_single_use_commands(
        const VulkanContext& context,
        VkCommandPool commandPool,
        VkCommandBuffer commandBuffer);

    VkDevice m_device {VK_NULL_HANDLE};
    VkImage m_image {VK_NULL_HANDLE};
    VkDeviceMemory m_memory {VK_NULL_HANDLE};
    VkImageView m_imageView {VK_NULL_HANDLE};
    VkExtent2D m_extent {0U, 0U};
    VkFormat m_format {VK_FORMAT_UNDEFINED};
    VkImageAspectFlags m_aspectMask {0U};
    std::uint32_t m_mipLevels {1U};
};

struct TexturePixelData final {
    VkExtent2D extent {0U, 0U};
    std::vector<std::uint8_t> pixels {};
};

[[nodiscard]] auto load_texture_pixels(const std::filesystem::path& path, bool srgb) -> TexturePixelData;
[[nodiscard]] auto generate_checkerboard_pixels() -> TexturePixelData;
[[nodiscard]] auto generate_blue_noise_normal_pixels() -> TexturePixelData;

[[nodiscard]] auto create_texture_from_pixels(
    const VulkanContext& context,
    const TexturePixelData& data,
    VkFormat format,
    std::string_view name) -> TextureImage;

class TextureSamplers final {
public:
    TextureSamplers() = default;
    TextureSamplers(const TextureSamplers&) = delete;
    TextureSamplers(TextureSamplers&& other) noexcept;
    auto operator=(const TextureSamplers&) -> TextureSamplers& = delete;
    auto operator=(TextureSamplers&& other) noexcept -> TextureSamplers&;
    ~TextureSamplers() noexcept;

    [[nodiscard]] static auto create(const VulkanContext& context) -> TextureSamplers;

    [[nodiscard]] auto albedo() const noexcept -> VkSampler;
    [[nodiscard]] auto normal() const noexcept -> VkSampler;
    [[nodiscard]] auto anisotropy_enabled() const noexcept -> bool;
    [[nodiscard]] auto max_anisotropy() const noexcept -> float;

private:
    explicit TextureSamplers(const VulkanContext& context);

    void initialise(const VulkanContext& context);
    void cleanup() noexcept;
    void move_from(TextureSamplers&& other) noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    VkSampler m_albedo {VK_NULL_HANDLE};
    VkSampler m_normal {VK_NULL_HANDLE};
    bool m_anisotropy {false};
    float m_maxAnisotropy {1.0F};
};

} // namespace vulkano
