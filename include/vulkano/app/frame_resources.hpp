#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

namespace vulkano::app {
class VulkanContext;

class FrameResources final {
public:
    explicit FrameResources(const VulkanContext& context);
    ~FrameResources() noexcept;

    FrameResources(const FrameResources&) = delete;
    FrameResources& operator=(const FrameResources&) = delete;
    FrameResources(FrameResources&&) noexcept = delete;
    FrameResources& operator=(FrameResources&&) noexcept = delete;

    [[nodiscard]] VkCommandPool command_pool() const noexcept;
    [[nodiscard]] const std::vector<VkCommandBuffer>& command_buffers() const noexcept;
    [[nodiscard]] std::size_t frames_in_flight() const noexcept;
    [[nodiscard]] VkSemaphore image_available_semaphore(std::size_t index) const noexcept;
    [[nodiscard]] VkSemaphore render_finished_semaphore(std::uint32_t imageIndex) const noexcept;
    [[nodiscard]] VkFence in_flight_fence(std::size_t index) const noexcept;
    [[nodiscard]] VkFence image_in_flight(std::uint32_t imageIndex) const noexcept;
    void set_image_in_flight(std::uint32_t imageIndex, VkFence fence) noexcept;
    void reset_fence(std::size_t index) const;

private:
    void create_command_resources();
    void create_synchronization_objects();

    const VulkanContext& m_context;
    VkCommandPool m_commandPool {VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_imageRenderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    std::vector<VkFence> m_imagesInFlight;

    static constexpr std::size_t FramesInFlight {2U};
};
} // namespace vulkano::app
