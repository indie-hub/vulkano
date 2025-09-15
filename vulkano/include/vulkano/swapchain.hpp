#pragma once

#include <vulkan/vulkan.h>

namespace vulkano {

class VulkanContext;

class Swapchain final {
public:
    explicit Swapchain(VulkanContext& ctx);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    void recreate();
    VkFormat image_format() const noexcept;
    VkExtent2D extent() const noexcept;

private:
    class Impl;
    Impl* m_impl; // pImpl with manual lifetime in cpp
};

} // namespace vulkano

