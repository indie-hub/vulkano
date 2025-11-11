#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

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
    SSAODescriptors(const VulkanContext& context, const SSAOGpuResources& resources);
    ~SSAODescriptors() noexcept;

    SSAODescriptors(const SSAODescriptors&) = delete;
    SSAODescriptors& operator=(const SSAODescriptors&) = delete;
    SSAODescriptors(SSAODescriptors&& other) noexcept;
    SSAODescriptors& operator=(SSAODescriptors&& other) noexcept;

    [[nodiscard]] VkDescriptorSetLayout layout() const noexcept;
    [[nodiscard]] VkDescriptorSet descriptor_set() const noexcept;

private:
    void destroy() noexcept;

    VkDevice m_device {VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool {VK_NULL_HANDLE};
    VkDescriptorSetLayout m_layout {VK_NULL_HANDLE};
    VkDescriptorSet m_descriptorSet {VK_NULL_HANDLE};
};
}
