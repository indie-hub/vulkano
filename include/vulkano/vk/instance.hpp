#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace vulkano::vk {
class Instance final {
public:
    Instance() noexcept = default;
    static Instance create();

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&& other) noexcept;
    Instance& operator=(Instance&& other) noexcept;
    ~Instance() noexcept;

    [[nodiscard]] VkInstance handle() const noexcept;
    [[nodiscard]] VkDebugUtilsMessengerEXT debug_messenger() const noexcept;
    [[nodiscard]] bool validation_layers_enabled() const noexcept;
    [[nodiscard]] const std::vector<const char*>& validation_layers() const noexcept;

private:
    Instance(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, bool validationLayersEnabled,
        std::vector<const char*> validationLayers) noexcept;

    VkInstance m_instance {VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debugMessenger {VK_NULL_HANDLE};
    bool m_validationLayersEnabled {false};
    std::vector<const char*> m_validationLayers {};
};
} // namespace vulkano::vk
