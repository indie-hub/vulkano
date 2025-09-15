#pragma once

#include <cstdint>
#include <memory>
#include <functional>

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

    // Optional injection point to render extra content (e.g., ImGui).
    // The callback receives a native command buffer pointer (VkCommandBuffer when Vulkan is enabled).
    void set_ui_renderer(const std::function<void(void* cmd_buffer)>& renderer) noexcept;

    // Update icosphere subdivision level; triggers CPU mesh rebuild and GPU reupload
    void set_subdivisions(std::uint32_t subdivisions) noexcept;

    struct Settings final {
        float normal_strength {1.0F};
        float shininess {64.0F};
        float light_intensity {1.0F};
        float light_pos[3] {3.0F, 3.0F, 3.0F};
        float light_color[3] {1.0F, 1.0F, 1.0F};
    };

    void set_settings(const Settings& s) noexcept;
    [[nodiscard]] Settings get_settings() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    friend struct VulkanContextAccess;
    friend struct VulkanContextInternalsHelper;
};

// Accessor to selected VulkanContext internals for tightly-coupled helpers (e.g., ImGui integration)
struct VulkanContextAccess final {
    static void* impl_ptr(VulkanContext& ctx) noexcept;          // returns pointer to internal Impl
    static void* instance(VulkanContext& ctx) noexcept;           // returns VkInstance
    static void* physical(VulkanContext& ctx) noexcept;           // returns VkPhysicalDevice
    static void* device(VulkanContext& ctx) noexcept;             // returns VkDevice
    static void* queue(VulkanContext& ctx) noexcept;              // returns VkQueue
    static std::uint32_t queue_family(VulkanContext& ctx) noexcept;
    static void* render_pass(VulkanContext& ctx) noexcept;        // returns VkRenderPass
    static std::uint32_t image_count(VulkanContext& ctx) noexcept;
};

} // namespace vulkano
