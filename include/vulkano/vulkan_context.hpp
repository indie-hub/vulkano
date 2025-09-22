#pragma once

#include <vulkano/app_config.hpp>
#include <vulkano/window.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace vulkano {

struct QueueSelection final {
    std::optional<std::uint32_t> graphicsIndex {};
    std::optional<std::uint32_t> presentIndex {};
};

class VulkanContext final {
public:
    VulkanContext();
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&& other) noexcept;
    auto operator=(const VulkanContext&) -> VulkanContext& = delete;
    auto operator=(VulkanContext&& other) noexcept -> VulkanContext&;
    ~VulkanContext() noexcept;

    [[nodiscard]] auto instance() const noexcept -> VkInstance;
    [[nodiscard]] auto surface() const noexcept -> VkSurfaceKHR;
    [[nodiscard]] auto physical_device() const noexcept -> VkPhysicalDevice;
    [[nodiscard]] auto device() const noexcept -> VkDevice;
    [[nodiscard]] auto graphics_queue() const noexcept -> VkQueue;
    [[nodiscard]] auto present_queue() const noexcept -> VkQueue;
    [[nodiscard]] auto graphics_queue_index() const noexcept -> std::uint32_t;
    [[nodiscard]] auto present_queue_index() const noexcept -> std::uint32_t;
    [[nodiscard]] auto device_properties() const noexcept -> VkPhysicalDeviceProperties;
    [[nodiscard]] auto validation_enabled() const noexcept -> bool;
    void set_object_name(VkObjectType type, std::uint64_t handle, const std::string& name) const;
    void begin_debug_label(VkCommandBuffer commandBuffer, const std::string& label) const;
    void end_debug_label(VkCommandBuffer commandBuffer) const;

private:
    friend class VulkanContextBuilder;

    explicit VulkanContext(const AppConfig& config, const Window& window);

    void create_instance();
    void setup_debug_messenger();
    void create_surface();
    void pick_physical_device();
    void create_logical_device();
    void load_debug_functions();

    [[nodiscard]] auto required_instance_extensions() const -> std::vector<const char*>;
    [[nodiscard]] auto check_validation_layer_support() const -> bool;
    [[nodiscard]] auto gather_queue_selection(VkPhysicalDevice candidate) const -> QueueSelection;
    [[nodiscard]] auto check_device_extension_support(VkPhysicalDevice candidate) const -> bool;

    void cleanup() noexcept;
    void move_from(VulkanContext&& other) noexcept;

    AppConfig m_config;
    const Window* m_window {nullptr};
    VkInstance m_instance {VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debugMessenger {VK_NULL_HANDLE};
    VkSurfaceKHR m_surface {VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice {VK_NULL_HANDLE};
    VkDevice m_device {VK_NULL_HANDLE};
    VkQueue m_graphicsQueue {VK_NULL_HANDLE};
    VkQueue m_presentQueue {VK_NULL_HANDLE};
    std::uint32_t m_graphicsQueueIndex {0U};
    std::uint32_t m_presentQueueIndex {0U};
    VkPhysicalDeviceProperties m_deviceProperties {};
    PFN_vkSetDebugUtilsObjectNameEXT m_setObjectName {nullptr};
    PFN_vkCmdBeginDebugUtilsLabelEXT m_cmdBeginLabel {nullptr};
    PFN_vkCmdEndDebugUtilsLabelEXT m_cmdEndLabel {nullptr};
};

class VulkanContextBuilder final {
public:
    VulkanContextBuilder();

    [[nodiscard]] auto with_config(const AppConfig& config) -> VulkanContextBuilder&;
    [[nodiscard]] auto with_window(const Window& window) -> VulkanContextBuilder&;
    [[nodiscard]] auto build() const -> VulkanContext;

private:
    const AppConfig* m_config;
    const Window* m_window;
};

} // namespace vulkano
