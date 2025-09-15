#include <vulkano/swapchain.hpp>
#include <vulkano/vulkan_context.hpp>

namespace vulkano {

class Swapchain::Impl {
public:
    VulkanContext& ctx;
    VkSwapchainKHR swapchain {VK_NULL_HANDLE};
    VkFormat format {VK_FORMAT_B8G8R8A8_UNORM};
    VkExtent2D extent {1280u, 720u};
    explicit Impl(VulkanContext& c) : ctx {c} {}
};

Swapchain::Swapchain(VulkanContext& ctx)
    : m_impl {new Impl(ctx)}
{
}

Swapchain::~Swapchain()
{
    delete m_impl;
}

void Swapchain::recreate()
{
    // Stub: actual implementation deferred
}

VkFormat Swapchain::image_format() const noexcept
{
    return m_impl->format;
}

VkExtent2D Swapchain::extent() const noexcept
{
    return m_impl->extent;
}

} // namespace vulkano

