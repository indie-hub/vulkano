#pragma once

#include <vulkano/render_pass.hpp>
#include <vulkano/swapchain.hpp>

#include <vector>
#include <vulkan/vulkan.h>

namespace vulkano {

class FramebufferCollection final {
public:
    FramebufferCollection() = default;
    FramebufferCollection(const FramebufferCollection&) = delete;
    FramebufferCollection(FramebufferCollection&& other) noexcept;
    auto operator=(const FramebufferCollection&) -> FramebufferCollection& = delete;
    auto operator=(FramebufferCollection&& other) noexcept -> FramebufferCollection&;
    ~FramebufferCollection() noexcept;

    [[nodiscard]] static auto create(const VulkanContext& context, const Swapchain& swapchain, const RenderPass& renderPass) -> FramebufferCollection;

    [[nodiscard]] auto handles() const noexcept -> const std::vector<VkFramebuffer>&;
    [[nodiscard]] auto size() const noexcept -> std::size_t;
    void recreate(const VulkanContext& context, const Swapchain& swapchain, const RenderPass& renderPass);

private:
    explicit FramebufferCollection(const VulkanContext& context, const Swapchain& swapchain, const RenderPass& renderPass);

    void initialise(const VulkanContext& context, const Swapchain& swapchain, const RenderPass& renderPass);
    void cleanup() noexcept;
    void move_from(FramebufferCollection&& other) noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    std::vector<VkFramebuffer> m_framebuffers {};
};

} // namespace vulkano
