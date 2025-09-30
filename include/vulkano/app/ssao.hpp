#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <vulkan/vulkan.h>

#include <vulkano/vk/color_image.hpp>

namespace vulkano::app {
class VulkanContext;

class SSAOSampleGenerator final {
public:
    explicit SSAOSampleGenerator(std::uint32_t seed = default_seed) noexcept;

    [[nodiscard]] std::vector<glm::vec3> generate_kernel(std::size_t sampleCount) const;
    [[nodiscard]] std::vector<glm::vec3> generate_noise(std::size_t sampleCount) const;

    static constexpr std::uint32_t default_seed {1337U};

private:
    [[nodiscard]] std::mt19937 create_engine(std::uint32_t offset = 0U) const noexcept;

    std::uint32_t m_seed {default_seed};
};

class SSAOGpuResources final {
public:
    SSAOGpuResources(const VulkanContext& context, const SSAOSampleGenerator& generator,
        std::size_t kernelSamples, std::uint32_t noiseDimension);
    ~SSAOGpuResources() noexcept;

    SSAOGpuResources(const SSAOGpuResources&) = delete;
    SSAOGpuResources& operator=(const SSAOGpuResources&) = delete;
    SSAOGpuResources(SSAOGpuResources&& other) noexcept;
    SSAOGpuResources& operator=(SSAOGpuResources&& other) noexcept;

    [[nodiscard]] VkBuffer kernel_buffer() const noexcept;
    [[nodiscard]] VkDeviceMemory kernel_memory() const noexcept;
    [[nodiscard]] std::size_t kernel_sample_count() const noexcept;

    [[nodiscard]] VkImageView noise_image_view() const noexcept;
    [[nodiscard]] VkSampler noise_sampler() const noexcept;
    [[nodiscard]] VkFormat noise_format() const noexcept;
    [[nodiscard]] std::uint32_t noise_dimension() const noexcept;

private:
    void destroy() noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    VkBuffer m_kernelBuffer {VK_NULL_HANDLE};
    VkDeviceMemory m_kernelMemory {VK_NULL_HANDLE};
    std::size_t m_kernelSampleCount {0U};

    vk::ColorImage m_noiseImage;
    VkSampler m_noiseSampler {VK_NULL_HANDLE};
    std::uint32_t m_noiseDimension {0U};
    VkFormat m_noiseFormat {VK_FORMAT_UNDEFINED};
};


class SSAODescriptors final {
public:
    SSAODescriptors(const VulkanContext& context, const SSAOGpuResources& resources, VkImageView normalView, VkImageView depthView);
    ~SSAODescriptors() noexcept;

    SSAODescriptors(const SSAODescriptors&) = delete;
    SSAODescriptors& operator=(const SSAODescriptors&) = delete;
    SSAODescriptors(SSAODescriptors&& other) noexcept;
    SSAODescriptors& operator=(SSAODescriptors&& other) noexcept;

    [[nodiscard]] VkDescriptorSetLayout layout() const noexcept;
    [[nodiscard]] VkDescriptorSet descriptor_set() const noexcept;
    void update_gbuffer_views(VkImageView normalView, VkImageView depthView);
    void set_camera_parameters(const glm::mat4& projection, const glm::mat4& inverseProjection, VkExtent2D extent,
        float radius, float bias, std::uint32_t noiseDimension, float angleCosThreshold, float depthFalloff,
        float distanceFalloff, float normalEpsilon) noexcept;

private:
    void destroy() noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool {VK_NULL_HANDLE};
    VkDescriptorSetLayout m_layout {VK_NULL_HANDLE};
    VkDescriptorSet m_descriptorSet {VK_NULL_HANDLE};
    VkSampler m_normalSampler {VK_NULL_HANDLE};
    VkSampler m_depthSampler {VK_NULL_HANDLE};
    VkBuffer m_paramsBuffer {VK_NULL_HANDLE};
    VkDeviceMemory m_paramsMemory {VK_NULL_HANDLE};

    struct ShaderParams final {
        glm::mat4 inverseProjection {1.0F};
        glm::mat4 projection {1.0F};
        glm::vec4 sampleParams {1.0F, 1.0F, 0.75F, 0.025F};
        glm::vec4 attenuationParams {0.3F, 2.0F, 1.0F, 0.0F};
    };

    ShaderParams m_params {};
};

class SSAOPass final {
public:
    SSAOPass(const VulkanContext& context, VkDescriptorSetLayout descriptorLayout, VkExtent2D extent);
    ~SSAOPass() noexcept;

    SSAOPass(const SSAOPass&) = delete;
    SSAOPass& operator=(const SSAOPass&) = delete;
    SSAOPass(SSAOPass&& other) noexcept;
    SSAOPass& operator=(SSAOPass&& other) noexcept;

    void resize(const VulkanContext& context, VkExtent2D extent);
    void record(VkCommandBuffer commandBuffer, const SSAODescriptors& descriptors) const;

    [[nodiscard]] VkImageView occlusion_view() const noexcept;
    [[nodiscard]] VkFormat occlusion_format() const noexcept;

private:
    void destroy() noexcept;
    void create_static_resources(const VulkanContext& context, VkDescriptorSetLayout descriptorLayout);
    void recreate_framebuffer(const VulkanContext& context, VkExtent2D extent);

    VkDevice m_device {VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice {VK_NULL_HANDLE};
    VkDescriptorSetLayout m_descriptorLayout {VK_NULL_HANDLE};
    VkExtent2D m_extent {0U, 0U};
    VkFormat m_occlusionFormat {VK_FORMAT_R16_SFLOAT};
    vk::ColorImage m_occlusionImage;
    VkRenderPass m_renderPass {VK_NULL_HANDLE};
    VkPipelineLayout m_pipelineLayout {VK_NULL_HANDLE};
    VkPipeline m_pipeline {VK_NULL_HANDLE};
    VkFramebuffer m_framebuffer {VK_NULL_HANDLE};
};

class SSAOBlurPass final {
public:
    SSAOBlurPass(const VulkanContext& context, VkExtent2D extent);
    ~SSAOBlurPass() noexcept;

    SSAOBlurPass(const SSAOBlurPass&) = delete;
    SSAOBlurPass& operator=(const SSAOBlurPass&) = delete;
    SSAOBlurPass(SSAOBlurPass&& other) noexcept;
    SSAOBlurPass& operator=(SSAOBlurPass&& other) noexcept;

    void resize(const VulkanContext& context, VkExtent2D extent);
    void set_depth_view(VkImageView depthView) noexcept;
    void set_occlusion_view(VkImageView occlusionView) noexcept;
    void set_parameters(float radius, float depthSigma) noexcept;
    void record(VkCommandBuffer commandBuffer, VkImageView occlusionView) const;

    [[nodiscard]] VkImageView blurred_view() const noexcept;

private:
    void destroy() noexcept;
    void create_resources(const VulkanContext& context);
    void recreate_framebuffer(const VulkanContext& context, VkExtent2D extent);
    void transition_blur_image(VkCommandBuffer commandBuffer, VkImageLayout oldLayout,
        VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) const;

    VkDevice m_device {VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice {VK_NULL_HANDLE};
    VkExtent2D m_extent {0U, 0U};

    VkDescriptorPool m_descriptorPool {VK_NULL_HANDLE};
    VkDescriptorSetLayout m_descriptorLayout {VK_NULL_HANDLE};
    VkDescriptorSet m_descriptorSet {VK_NULL_HANDLE};
    VkSampler m_occlusionSampler {VK_NULL_HANDLE};
    VkSampler m_depthSampler {VK_NULL_HANDLE};
    VkBuffer m_paramsBuffer {VK_NULL_HANDLE};
    VkDeviceMemory m_paramsMemory {VK_NULL_HANDLE};

    VkRenderPass m_renderPass {VK_NULL_HANDLE};
    VkPipelineLayout m_pipelineLayout {VK_NULL_HANDLE};
    VkPipeline m_pipeline {VK_NULL_HANDLE};
    VkFramebuffer m_framebuffer {VK_NULL_HANDLE};
    vk::ColorImage m_blurImage;

    VkImageView m_depthView {VK_NULL_HANDLE};
    VkImageView m_occlusionView {VK_NULL_HANDLE};

    struct BlurUniform final {
        float radius {2.0F};
        float depthSigma {0.1F};
        glm::vec2 padding {0.0F};
    };

    BlurUniform m_params {};
};

class SSAOCompositeDescriptors final {
public:
    SSAOCompositeDescriptors(const VulkanContext& context, VkImageView initialOcclusionView = VK_NULL_HANDLE);
    ~SSAOCompositeDescriptors() noexcept;

    SSAOCompositeDescriptors(const SSAOCompositeDescriptors&) = delete;
    SSAOCompositeDescriptors& operator=(const SSAOCompositeDescriptors&) = delete;
    SSAOCompositeDescriptors(SSAOCompositeDescriptors&& other) noexcept;
    SSAOCompositeDescriptors& operator=(SSAOCompositeDescriptors&& other) noexcept;

    void update_occlusion_view(VkImageView view);
    void set_config(float strength, float baseAmbient, bool debugView = false);

    [[nodiscard]] VkDescriptorSetLayout layout() const noexcept;
    [[nodiscard]] VkDescriptorSet descriptor_set() const noexcept;

private:
    void destroy() noexcept;
    void write_config() const;

    struct Config final {
        float occlusionStrength {1.0F};
        float baseAmbient {0.2F};
        float debugView {0.0F};
        float padding {0.0F};
    };

    VkDevice m_device {VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool {VK_NULL_HANDLE};
    VkDescriptorSetLayout m_layout {VK_NULL_HANDLE};
    VkDescriptorSet m_descriptorSet {VK_NULL_HANDLE};
    VkBuffer m_configBuffer {VK_NULL_HANDLE};
    VkDeviceMemory m_configMemory {VK_NULL_HANDLE};
    VkSampler m_occlusionSampler {VK_NULL_HANDLE};
    Config m_config {};
    VkImageView m_currentOcclusionView {VK_NULL_HANDLE};
};
}
