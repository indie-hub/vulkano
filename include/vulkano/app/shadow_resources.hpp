#pragma once

#include <vector>

#include <vulkano/app/shadow_map.hpp>
#include <vulkano/app/shadow_pass.hpp>
#include <vulkano/scene/light.hpp>

namespace vulkano::app {
class VulkanContext;

struct ShadowSlot final {
    scene::LightId id {scene::LightId::invalid()};
    ShadowMap map;
    ShadowPass pass;
    bool active {false};
    bool dirtyMatrix {true};
    glm::mat4 viewProjection {1.0F};
    std::uint32_t priority {0U};
};

class ShadowResources final {
public:
    void initialise(const VulkanContext& context, VkExtent2D extent, VkFormat format, std::size_t maxSlots);
    void release(const VulkanContext& context) noexcept;
    void update_descriptors(const VulkanContext& context);

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t slot_count() const noexcept;
    [[nodiscard]] ShadowSlot& slot(std::size_t index);
    [[nodiscard]] const ShadowSlot& slot(std::size_t index) const;
    [[nodiscard]] VkDescriptorSetLayout descriptor_layout() const noexcept;
    [[nodiscard]] VkDescriptorSet descriptor_set() const noexcept;

    std::vector<ShadowSlot> slots;
    VkDescriptorSetLayout descriptorLayout {VK_NULL_HANDLE};
    VkDescriptorPool descriptorPool {VK_NULL_HANDLE};
    VkDescriptorSet descriptorSet {VK_NULL_HANDLE};
    VkExtent2D extent {2048U, 2048U};
    VkFormat format {VK_FORMAT_D32_SFLOAT};
    std::uint32_t activeCount {0U};
};
} // namespace vulkano::app
