#pragma once

#include <cstdint>
#include <memory>

namespace vulkano {

class Window;

class VulkanContext final {
public:
    VulkanContext() = delete;
    explicit VulkanContext(const Window& window);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) noexcept;
    VulkanContext& operator=(VulkanContext&&) noexcept;

    void wait_idle() const noexcept;
    void draw_frame();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    friend struct VulkanContextAccess;
};

} // namespace vulkano
