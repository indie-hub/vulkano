#pragma once

#include <vulkano/vulkan_context.hpp>

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

namespace vulkano {

class CommandAllocator final {
public:
    CommandAllocator() = default;
    CommandAllocator(const CommandAllocator&) = delete;
    CommandAllocator(CommandAllocator&& other) noexcept;
    auto operator=(const CommandAllocator&) -> CommandAllocator& = delete;
    auto operator=(CommandAllocator&& other) noexcept -> CommandAllocator&;
    ~CommandAllocator() noexcept;

    [[nodiscard]] static auto create(const VulkanContext& context, std::uint32_t bufferCount) -> CommandAllocator;

    [[nodiscard]] auto pool() const noexcept -> VkCommandPool;
    [[nodiscard]] auto buffers() const noexcept -> const std::vector<VkCommandBuffer>&;
    [[nodiscard]] auto buffer_count() const noexcept -> std::uint32_t;

    void reset() const;
    void recreate(const VulkanContext& context, std::uint32_t bufferCount);

private:
    explicit CommandAllocator(const VulkanContext& context, std::uint32_t bufferCount);

    void initialise(const VulkanContext& context, std::uint32_t bufferCount);
    void cleanup() noexcept;
    void allocate_buffers(std::uint32_t bufferCount);
    void move_from(CommandAllocator&& other) noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    VkCommandPool m_commandPool {VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> m_commandBuffers {};
    std::uint32_t m_queueFamilyIndex {0U};
};

} // namespace vulkano
