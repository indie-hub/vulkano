#pragma once

#include <cstdint>
#include <vector>

#include <vulkano/vulkan_context.hpp>

#include <vulkan/vulkan.h>

namespace vulkano {

class DepthResources final {
public:
    DepthResources() = default;
    DepthResources(const DepthResources&) = delete;
    DepthResources(DepthResources&& other) noexcept;
    auto operator=(const DepthResources&) -> DepthResources& = delete;
    auto operator=(DepthResources&& other) noexcept -> DepthResources&;
    ~DepthResources() noexcept;

    [[nodiscard]] static auto find_supported_format(const VulkanContext& context) -> VkFormat;
    [[nodiscard]] static auto create(const VulkanContext& context, VkFormat format, VkExtent2D extent, std::uint32_t imageCount) -> DepthResources;

    void recreate(const VulkanContext& context, VkFormat format, VkExtent2D extent, std::uint32_t imageCount);

    [[nodiscard]] auto format() const noexcept -> VkFormat;
    [[nodiscard]] auto image_views() const noexcept -> const std::vector<VkImageView>&;

private:
    DepthResources(const VulkanContext& context, VkFormat format, VkExtent2D extent, std::uint32_t imageCount);

    void initialise(const VulkanContext& context, VkFormat format, VkExtent2D extent, std::uint32_t imageCount);
    void cleanup() noexcept;
    void move_from(DepthResources&& other) noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    VkFormat m_format {VK_FORMAT_D32_SFLOAT};
    std::vector<VkImage> m_images {};
    std::vector<VkDeviceMemory> m_memories {};
    std::vector<VkImageView> m_imageViews {};
};

} // namespace vulkano

