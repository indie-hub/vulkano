#pragma once

#include <cstdint>

#include <vulkano/vulkan_context.hpp>

#include <vulkan/vulkan.h>

namespace vulkano {

class ShadowMap final {
public:
    ShadowMap() = default;
    ShadowMap(const ShadowMap&) = delete;
    ShadowMap(ShadowMap&& other) noexcept;
    auto operator=(const ShadowMap&) -> ShadowMap& = delete;
    auto operator=(ShadowMap&& other) noexcept -> ShadowMap&;
    ~ShadowMap() noexcept;

    static constexpr std::uint32_t defaultResolution {2048U};
    static constexpr std::uint32_t maxLayers {4U};

    [[nodiscard]] static auto create(
        const VulkanContext& context,
        std::uint32_t resolution,
        std::uint32_t layers) -> ShadowMap;

    void recreate(const VulkanContext& context, std::uint32_t resolution, std::uint32_t layers);

    [[nodiscard]] auto resolution() const noexcept -> std::uint32_t;
    [[nodiscard]] auto format() const noexcept -> VkFormat;
    [[nodiscard]] auto image() const noexcept -> VkImage;
    [[nodiscard]] auto image_view() const noexcept -> VkImageView;
    [[nodiscard]] auto layered_view() const noexcept -> VkImageView;
    [[nodiscard]] auto layer_views() const noexcept -> const std::vector<VkImageView>&;
    [[nodiscard]] auto layer_count() const noexcept -> std::uint32_t;
    [[nodiscard]] auto sampler() const noexcept -> VkSampler;

private:
    ShadowMap(const VulkanContext& context, std::uint32_t resolution, std::uint32_t layers);

    void initialise(const VulkanContext& context, std::uint32_t resolution, std::uint32_t layers);
    void cleanup() noexcept;
    void move_from(ShadowMap&& other) noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    VkImage m_image {VK_NULL_HANDLE};
    VkDeviceMemory m_memory {VK_NULL_HANDLE};
    VkImageView m_layeredView {VK_NULL_HANDLE};
    std::vector<VkImageView> m_layerViews {};
    VkSampler m_sampler {VK_NULL_HANDLE};
    VkFormat m_format {VK_FORMAT_D32_SFLOAT};
    std::uint32_t m_resolution {0U};
    std::uint32_t m_layers {0U};
};

} // namespace vulkano
