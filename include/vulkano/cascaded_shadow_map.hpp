#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <vulkan/vulkan.h>

#include <vulkano/shadow_render_pass.hpp>
#include <vulkano/vulkan_context.hpp>

namespace vulkano {

constexpr std::uint32_t maxShadowCascades {4U};

struct CascadedShadowSettings final {
    CascadedShadowSettings();

    bool enabled;
    bool stabilize;
    bool visualizeCascades;
    bool showShadowAtlas;
    std::uint32_t cascadeCount;
    std::uint32_t resolution;
    float splitLambda;
    float biasConstant;
    float biasSlope;
    float normalBias;
    float shadowStrength;
    float pcfRadius;
};

struct alignas(16) CascadedShadowUniform final {
    CascadedShadowUniform();

    std::array<glm::mat4, maxShadowCascades> lightViewProjection;
    std::array<glm::vec4, maxShadowCascades> cascadeData;
    glm::vec4 cascadeSplits;
    glm::vec4 shadowParams;
    glm::vec4 biasParams;
    glm::vec4 atlasSize;
    glm::vec4 debugParams;
};

struct ShadowComputationInput final {
    ShadowComputationInput();

    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 lightDirection;
    float nearPlane;
    float farPlane;
};

struct ShadowCascadeData final {
    ShadowCascadeData();

    CascadedShadowUniform uniform;
    std::array<float, maxShadowCascades> cascadeTexelSizes;
    std::array<glm::vec3, maxShadowCascades> cascadeHalfExtents;
    std::array<glm::vec3, maxShadowCascades> cascadeCenters;
    std::array<glm::vec3, maxShadowCascades> lightPositions;
    std::array<float, maxShadowCascades> cascadeBlendDistances;
    std::array<glm::vec3, maxShadowCascades> cascadeMinBounds;
    std::array<glm::vec3, maxShadowCascades> cascadeMaxBounds;
    std::array<glm::mat4, maxShadowCascades> lightViewMatrices;
    std::array<float, maxShadowCascades> cascadeNearPlanes;
    std::array<float, maxShadowCascades> cascadeFarPlanes;
};

auto compute_cascaded_shadow_data(
    const ShadowComputationInput& input,
    const CascadedShadowSettings& settings,
    std::uint32_t cascadeCount,
    std::uint32_t resolution) -> ShadowCascadeData;

class CascadedShadowMapResources final {
public:
    CascadedShadowMapResources() = default;
    CascadedShadowMapResources(const CascadedShadowMapResources&) = delete;
    CascadedShadowMapResources(CascadedShadowMapResources&& other) noexcept;
    auto operator=(const CascadedShadowMapResources&) -> CascadedShadowMapResources& = delete;
    auto operator=(CascadedShadowMapResources&& other) noexcept -> CascadedShadowMapResources&;
    ~CascadedShadowMapResources() noexcept;

    [[nodiscard]] static auto create(
        const VulkanContext& context,
        const ShadowRenderPass& renderPass,
        VkFormat format,
        std::uint32_t cascadeCount,
        std::uint32_t resolution) -> CascadedShadowMapResources;

    void recreate(
        const VulkanContext& context,
        const ShadowRenderPass& renderPass,
        VkFormat format,
        std::uint32_t cascadeCount,
        std::uint32_t resolution);

    void cleanup() noexcept;

    [[nodiscard]] auto cascade_count() const noexcept -> std::uint32_t;
    [[nodiscard]] auto extent() const noexcept -> VkExtent2D;
    [[nodiscard]] auto image() const noexcept -> VkImage;
    [[nodiscard]] auto image_view_array() const noexcept -> VkImageView;
    [[nodiscard]] auto layer_view(std::size_t index) const noexcept -> VkImageView;
    [[nodiscard]] auto framebuffer(std::size_t index) const noexcept -> VkFramebuffer;
    [[nodiscard]] auto sampler() const noexcept -> VkSampler;

private:
    CascadedShadowMapResources(
        const VulkanContext& context,
        const ShadowRenderPass& renderPass,
        VkFormat format,
        std::uint32_t cascadeCount,
        std::uint32_t resolution);

    void initialise(
        const VulkanContext& context,
        const ShadowRenderPass& renderPass,
        VkFormat format,
        std::uint32_t cascadeCount,
        std::uint32_t resolution);
    void move_from(CascadedShadowMapResources&& other) noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    VkFormat m_format {VK_FORMAT_D32_SFLOAT};
    VkImage m_image {VK_NULL_HANDLE};
    VkDeviceMemory m_memory {VK_NULL_HANDLE};
    VkImageView m_arrayView {VK_NULL_HANDLE};
    std::vector<VkImageView> m_layerViews {};
    std::vector<VkFramebuffer> m_framebuffers {};
    VkSampler m_sampler {VK_NULL_HANDLE};
    VkExtent2D m_extent {0U, 0U};
    std::uint32_t m_cascadeCount {0U};
};

namespace detail {
    auto compute_cascade_splits(
        float nearPlane,
        float farPlane,
        std::uint32_t cascadeCount,
        float lambda) -> std::array<float, maxShadowCascades>;
}

} // namespace vulkano
