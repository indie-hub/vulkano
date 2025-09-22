#pragma once

#include <vulkano/vulkan_context.hpp>

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

namespace vulkano {

struct FrameSync final {
    VkSemaphore imageAvailable {VK_NULL_HANDLE};
    VkSemaphore renderFinished {VK_NULL_HANDLE};
    VkFence inFlight {VK_NULL_HANDLE};
};

class SyncManager final {
public:
    SyncManager() = default;
    SyncManager(const SyncManager&) = delete;
    SyncManager(SyncManager&& other) noexcept;
    auto operator=(const SyncManager&) -> SyncManager& = delete;
    auto operator=(SyncManager&& other) noexcept -> SyncManager&;
    ~SyncManager() noexcept;

    [[nodiscard]] static auto create(const VulkanContext& context, std::uint32_t framesInFlight) -> SyncManager;

    [[nodiscard]] auto frames() const noexcept -> const std::vector<FrameSync>&;
    [[nodiscard]] auto frame_count() const noexcept -> std::uint32_t;

private:
    explicit SyncManager(const VulkanContext& context, std::uint32_t framesInFlight);

    void initialise(const VulkanContext& context, std::uint32_t framesInFlight);
    void cleanup() noexcept;
    void move_from(SyncManager&& other) noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    std::vector<FrameSync> m_frames {};
};

} // namespace vulkano
