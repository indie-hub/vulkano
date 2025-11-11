#include <vulkano/app/ssao.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec4.hpp>

#include <vulkano/app/vulkan_context.hpp>

namespace vulkano::app {
namespace {
[[nodiscard]] float lerp(float a, float b, float t) noexcept {
    return std::lerp(a, b, t);
}

[[nodiscard]] std::uint32_t find_memory_type(VkPhysicalDevice physicalDevice, std::uint32_t typeFilter,
    VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties {};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (std::uint32_t i {0U}; i < memoryProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1U << i)) != 0U
            && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error {"Failed to find suitable memory type"};
}

[[nodiscard]] VkBuffer create_buffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize size,
    VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer {VK_NULL_HANDLE};
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create buffer"};
    }

    VkMemoryRequirements requirements {};
    vkGetBufferMemoryRequirements(device, buffer, &requirements);

    VkMemoryAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = find_memory_type(physicalDevice, requirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocateInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        throw std::runtime_error {"Failed to allocate buffer memory"};
    }

    if (vkBindBufferMemory(device, buffer, memory, 0U) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
        throw std::runtime_error {"Failed to bind buffer memory"};
    }

    return buffer;
}

void copy_to_memory(VkDevice device, VkDeviceMemory memory, const void* data, VkDeviceSize size) {
    void* mapped = nullptr;
    if (vkMapMemory(device, memory, 0U, size, 0U, &mapped) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to map memory"};
    }
    std::memcpy(mapped, data, static_cast<std::size_t>(size));
    vkUnmapMemory(device, memory);
}

void submit_and_wait(VkQueue queue, VkCommandBuffer commandBuffer) {
    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1U;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(queue, 1U, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to submit SSAO upload commands"};
    }
    if (vkQueueWaitIdle(queue) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to wait for SSAO upload queue"};
    }
}
}

SSAOSampleGenerator::SSAOSampleGenerator(std::uint32_t seed) noexcept
    : m_seed {seed} {
}

std::mt19937 SSAOSampleGenerator::create_engine(std::uint32_t offset) const noexcept {
    return std::mt19937 {m_seed + offset};
}

std::vector<glm::vec3> SSAOSampleGenerator::generate_kernel(std::size_t sampleCount) const {
    if (sampleCount == 0U) {
        return {};
    }

    std::vector<glm::vec3> kernel;
    kernel.reserve(sampleCount);

    std::mt19937 engine = create_engine();
    std::uniform_real_distribution<float> randomFloats {0.0F, 1.0F};

    for (std::size_t i {0U}; i < sampleCount; ++i) {
        glm::vec3 sample {
            randomFloats(engine) * 2.0F - 1.0F,
            randomFloats(engine) * 2.0F - 1.0F,
            randomFloats(engine)
        };
        sample = glm::normalize(sample);

        const float progress = static_cast<float>(i) / static_cast<float>(sampleCount);
        const float scale = lerp(0.1F, 1.0F, progress * progress);
        sample *= scale;
        kernel.push_back(sample);
    }

    return kernel;
}

std::vector<glm::vec3> SSAOSampleGenerator::generate_noise(std::size_t sampleCount) const {
    if (sampleCount == 0U) {
        return {};
    }

    std::vector<glm::vec3> noise;
    noise.reserve(sampleCount);

    std::mt19937 engine = create_engine(1U);
    std::uniform_real_distribution<float> randomFloats {0.0F, 1.0F};

    for (std::size_t i {0U}; i < sampleCount; ++i) {
        glm::vec3 vector {
            randomFloats(engine) * 2.0F - 1.0F,
            randomFloats(engine) * 2.0F - 1.0F,
            0.0F
        };

        if (glm::length(vector) > 0.0F) {
            vector = glm::normalize(vector);
        } else {
            vector = glm::vec3 {1.0F, 0.0F, 0.0F};
        }

        vector *= randomFloats(engine);
        noise.push_back(vector);
    }

    return noise;
}

SSAOGpuResources::SSAOGpuResources(const VulkanContext& context, const SSAOSampleGenerator& generator,
    std::size_t kernelSamples, std::uint32_t noiseDimension)
    : m_device {context.device()} {
    if (kernelSamples == 0U) {
        throw std::invalid_argument {"SSAO kernel requires at least one sample"};
    }
    if (noiseDimension == 0U) {
        throw std::invalid_argument {"SSAO noise texture dimension must be positive"};
    }

    const VkPhysicalDevice physicalDevice = context.physical_device();
    const VkDevice device = context.device();
    const VkQueue queue = context.graphics_queue();
    const std::uint32_t queueFamily = context.graphics_queue_family_index();

    try {
        const auto kernelSamplesData = generator.generate_kernel(kernelSamples);
        std::vector<glm::vec4> kernelUpload;
        kernelUpload.reserve(kernelSamplesData.size());
        for (const glm::vec3& sample : kernelSamplesData) {
            kernelUpload.emplace_back(sample, 0.0F);
        }

        const VkDeviceSize kernelSize = static_cast<VkDeviceSize>(sizeof(glm::vec4) * kernelUpload.size());
        m_kernelBuffer = create_buffer(physicalDevice, device, kernelSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_kernelMemory);
        copy_to_memory(device, m_kernelMemory, kernelUpload.data(), kernelSize);
        m_kernelSampleCount = kernelUpload.size();

        const std::size_t noiseSampleCount = static_cast<std::size_t>(noiseDimension) * static_cast<std::size_t>(noiseDimension);
        const auto noiseSamples = generator.generate_noise(noiseSampleCount);
        std::vector<glm::vec4> noiseUpload;
        noiseUpload.reserve(noiseSamples.size());
        for (const glm::vec3& n : noiseSamples) {
            noiseUpload.emplace_back(n.x, n.y, 0.0F, 0.0F);
        }
        const VkDeviceSize noiseSize = static_cast<VkDeviceSize>(sizeof(glm::vec4) * noiseUpload.size());

        VkDeviceMemory stagingMemory {VK_NULL_HANDLE};
        VkBuffer stagingBuffer = create_buffer(physicalDevice, device, noiseSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingMemory);
        copy_to_memory(device, stagingMemory, noiseUpload.data(), noiseSize);

        m_noiseFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
        m_noiseDimension = noiseDimension;
        const VkExtent2D noiseExtent {noiseDimension, noiseDimension};
        m_noiseImage = vk::ColorImage::create(physicalDevice, device, noiseExtent, m_noiseFormat,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

        VkCommandPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex = queueFamily;

        VkCommandPool commandPool {VK_NULL_HANDLE};
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingMemory, nullptr);
            throw std::runtime_error {"Failed to create SSAO upload command pool"};
        }

        VkCommandBufferAllocateInfo allocateInfo {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1U;

        VkCommandBuffer commandBuffer {VK_NULL_HANDLE};
        if (vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer) != VK_SUCCESS) {
            vkDestroyCommandPool(device, commandPool, nullptr);
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingMemory, nullptr);
            throw std::runtime_error {"Failed to allocate SSAO upload command buffer"};
        }

        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            vkDestroyCommandPool(device, commandPool, nullptr);
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingMemory, nullptr);
            throw std::runtime_error {"Failed to begin SSAO upload command buffer"};
        }

        VkImageMemoryBarrier toTransfer {};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcAccessMask = 0U;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.image = m_noiseImage.image();
        toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.baseMipLevel = 0U;
        toTransfer.subresourceRange.levelCount = 1U;
        toTransfer.subresourceRange.baseArrayLayer = 0U;
        toTransfer.subresourceRange.layerCount = 1U;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0U,
            0U, nullptr, 0U, nullptr, 1U, &toTransfer);

        VkBufferImageCopy copyRegion {};
        copyRegion.bufferOffset = 0U;
        copyRegion.bufferRowLength = 0U;
        copyRegion.bufferImageHeight = 0U;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0U;
        copyRegion.imageSubresource.baseArrayLayer = 0U;
        copyRegion.imageSubresource.layerCount = 1U;
        copyRegion.imageOffset = {0, 0, 0};
        copyRegion.imageExtent = {noiseDimension, noiseDimension, 1U};

        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, m_noiseImage.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1U, &copyRegion);

        VkImageMemoryBarrier toShaderRead {};
        toShaderRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toShaderRead.image = m_noiseImage.image();
        toShaderRead.subresourceRange = toTransfer.subresourceRange;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0U,
            0U, nullptr, 0U, nullptr, 1U, &toShaderRead);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            vkDestroyCommandPool(device, commandPool, nullptr);
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingMemory, nullptr);
            throw std::runtime_error {"Failed to record SSAO upload commands"};
        }

        submit_and_wait(queue, commandBuffer);

        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);

        VkSamplerCreateInfo samplerInfo {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &m_noiseSampler) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create SSAO noise sampler"};
        }
    } catch (...) {
        destroy();
        throw;
    }
}

SSAOGpuResources::~SSAOGpuResources() noexcept {
    destroy();
}

SSAOGpuResources::SSAOGpuResources(SSAOGpuResources&& other) noexcept {
    *this = std::move(other);
}

SSAOGpuResources& SSAOGpuResources::operator=(SSAOGpuResources&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy();

    m_device = other.m_device;
    m_kernelBuffer = other.m_kernelBuffer;
    m_kernelMemory = other.m_kernelMemory;
    m_kernelSampleCount = other.m_kernelSampleCount;
    m_noiseImage = std::move(other.m_noiseImage);
    m_noiseSampler = other.m_noiseSampler;
    m_noiseDimension = other.m_noiseDimension;
    m_noiseFormat = other.m_noiseFormat;

    other.m_device = VK_NULL_HANDLE;
    other.m_kernelBuffer = VK_NULL_HANDLE;
    other.m_kernelMemory = VK_NULL_HANDLE;
    other.m_kernelSampleCount = 0U;
    other.m_noiseSampler = VK_NULL_HANDLE;
    other.m_noiseDimension = 0U;
    other.m_noiseFormat = VK_FORMAT_UNDEFINED;

    return *this;
}

VkBuffer SSAOGpuResources::kernel_buffer() const noexcept {
    return m_kernelBuffer;
}

VkDeviceMemory SSAOGpuResources::kernel_memory() const noexcept {
    return m_kernelMemory;
}

std::size_t SSAOGpuResources::kernel_sample_count() const noexcept {
    return m_kernelSampleCount;
}

VkImageView SSAOGpuResources::noise_image_view() const noexcept {
    return m_noiseImage.view();
}

VkSampler SSAOGpuResources::noise_sampler() const noexcept {
    return m_noiseSampler;
}

VkFormat SSAOGpuResources::noise_format() const noexcept {
    return m_noiseFormat;
}

std::uint32_t SSAOGpuResources::noise_dimension() const noexcept {
    return m_noiseDimension;
}

void SSAOGpuResources::destroy() noexcept {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }

    if (m_kernelBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_kernelBuffer, nullptr);
        m_kernelBuffer = VK_NULL_HANDLE;
    }
    if (m_kernelMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_kernelMemory, nullptr);
        m_kernelMemory = VK_NULL_HANDLE;
    }

    if (m_noiseSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_noiseSampler, nullptr);
        m_noiseSampler = VK_NULL_HANDLE;
    }

    m_noiseImage = vk::ColorImage {};

    m_device = VK_NULL_HANDLE;
    m_kernelSampleCount = 0U;
    m_noiseDimension = 0U;
    m_noiseFormat = VK_FORMAT_UNDEFINED;
}

SSAODescriptors::SSAODescriptors(const VulkanContext& context, const SSAOGpuResources& resources, VkImageView normalView, VkImageView depthView)
    : m_device {context.device()} {
    try {
        const VkDevice device = m_device;

        VkDescriptorPoolSize poolSizes[2] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1U},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3U}
        };

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 2U;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = 1U;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create SSAO descriptor pool"};
        }

        VkDescriptorSetLayoutBinding bindings[4] = {};
        bindings[0].binding = 0U;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1U;
        bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        bindings[1].binding = 1U;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount = 1U;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        bindings[2].binding = 2U;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[2].descriptorCount = 1U;
        bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        bindings[3].binding = 3U;
        bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[3].descriptorCount = 1U;
        bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 4U;
        layoutInfo.pBindings = bindings;

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create SSAO descriptor set layout"};
        }

        VkDescriptorSetAllocateInfo allocateInfo {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = m_descriptorPool;
        allocateInfo.descriptorSetCount = 1U;
        allocateInfo.pSetLayouts = &m_layout;

        if (vkAllocateDescriptorSets(device, &allocateInfo, &m_descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to allocate SSAO descriptor set"};
        }

        VkSamplerCreateInfo normalSamplerInfo {};
        normalSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        normalSamplerInfo.magFilter = VK_FILTER_LINEAR;
        normalSamplerInfo.minFilter = VK_FILTER_LINEAR;
        normalSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        normalSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        normalSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        normalSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

        if (vkCreateSampler(device, &normalSamplerInfo, nullptr, &m_normalSampler) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create SSAO normal sampler"};
        }

        VkSamplerCreateInfo depthSamplerInfo = normalSamplerInfo;
        depthSamplerInfo.magFilter = VK_FILTER_NEAREST;
        depthSamplerInfo.minFilter = VK_FILTER_NEAREST;

        if (vkCreateSampler(device, &depthSamplerInfo, nullptr, &m_depthSampler) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create SSAO depth sampler"};
        }

        VkDescriptorBufferInfo bufferInfo {};
        bufferInfo.buffer = resources.kernel_buffer();
        bufferInfo.offset = 0U;
        bufferInfo.range = VK_WHOLE_SIZE;

        VkDescriptorImageInfo noiseInfo {};
        noiseInfo.sampler = resources.noise_sampler();
        noiseInfo.imageView = resources.noise_image_view();
        noiseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        std::array<VkWriteDescriptorSet, 2> initialWrites {
            VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = m_descriptorSet,
                .dstBinding = 0U,
                .dstArrayElement = 0U,
                .descriptorCount = 1U,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pImageInfo = nullptr,
                .pBufferInfo = &bufferInfo,
                .pTexelBufferView = nullptr
            },
            VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = m_descriptorSet,
                .dstBinding = 1U,
                .dstArrayElement = 0U,
                .descriptorCount = 1U,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &noiseInfo,
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr
            }
        };

        vkUpdateDescriptorSets(device, static_cast<std::uint32_t>(initialWrites.size()), initialWrites.data(), 0U, nullptr);

        update_gbuffer_views(normalView, depthView);
    } catch (...) {
        destroy();
        throw;
    }
}

SSAODescriptors::~SSAODescriptors() noexcept {
    destroy();
}

SSAODescriptors::SSAODescriptors(SSAODescriptors&& other) noexcept {
    *this = std::move(other);
}

SSAODescriptors& SSAODescriptors::operator=(SSAODescriptors&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy();

    m_device = other.m_device;
    m_descriptorPool = other.m_descriptorPool;
    m_layout = other.m_layout;
    m_descriptorSet = other.m_descriptorSet;
    m_normalSampler = other.m_normalSampler;
    m_depthSampler = other.m_depthSampler;

    other.m_device = VK_NULL_HANDLE;
    other.m_descriptorPool = VK_NULL_HANDLE;
    other.m_layout = VK_NULL_HANDLE;
    other.m_descriptorSet = VK_NULL_HANDLE;
    other.m_normalSampler = VK_NULL_HANDLE;
    other.m_depthSampler = VK_NULL_HANDLE;

    return *this;
}

VkDescriptorSetLayout SSAODescriptors::layout() const noexcept {
    return m_layout;
}

VkDescriptorSet SSAODescriptors::descriptor_set() const noexcept {
    return m_descriptorSet;
}

void SSAODescriptors::update_gbuffer_views(VkImageView normalView, VkImageView depthView) {
    if (m_device == VK_NULL_HANDLE || m_descriptorSet == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorImageInfo normalInfo {};
    normalInfo.sampler = m_normalSampler;
    normalInfo.imageView = normalView;
    normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo depthInfo {};
    depthInfo.sampler = m_depthSampler;
    depthInfo.imageView = depthView;
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array<VkWriteDescriptorSet, 2> writes {
        VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_descriptorSet,
            .dstBinding = 2U,
            .dstArrayElement = 0U,
            .descriptorCount = 1U,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &normalInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        VkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_descriptorSet,
            .dstBinding = 3U,
            .dstArrayElement = 0U,
            .descriptorCount = 1U,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &depthInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        }
    };

    vkUpdateDescriptorSets(m_device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0U, nullptr);
}

void SSAODescriptors::destroy() noexcept {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }
    if (m_normalSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_normalSampler, nullptr);
        m_normalSampler = VK_NULL_HANDLE;
    }
    if (m_depthSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_depthSampler, nullptr);
        m_depthSampler = VK_NULL_HANDLE;
    }

    m_descriptorSet = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
}


namespace {
[[nodiscard]] std::filesystem::path shader_directory() {
    return std::filesystem::current_path() / "bin" / "shaders";
}

[[nodiscard]] std::vector<char> read_binary_file(const std::filesystem::path& path) {
    std::ifstream file {path, std::ios::binary | std::ios::ate};
    if (!file.is_open()) {
        throw std::runtime_error {"Unable to open shader file: " + path.string()};
    }
    const std::streamsize size = file.tellg();
    std::vector<char> buffer(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!file.read(buffer.data(), size)) {
        throw std::runtime_error {"Failed to read shader file: " + path.string()};
    }
    return buffer;
}

[[nodiscard]] VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());

    VkShaderModule module {VK_NULL_HANDLE};
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create shader module"};
    }
    return module;
}
}

SSAOPass::SSAOPass(const VulkanContext& context, VkDescriptorSetLayout descriptorLayout, VkExtent2D extent)
    : m_device {context.device()}
    , m_physicalDevice {context.physical_device()}
    , m_descriptorLayout {descriptorLayout}
    , m_extent {extent} {
    create_static_resources(context, descriptorLayout);
    recreate_framebuffer(context, extent);
}

SSAOPass::~SSAOPass() noexcept {
    destroy();
}

SSAOPass::SSAOPass(SSAOPass&& other) noexcept {
    *this = std::move(other);
}

SSAOPass& SSAOPass::operator=(SSAOPass&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy();

    m_device = other.m_device;
    m_physicalDevice = other.m_physicalDevice;
    m_descriptorLayout = other.m_descriptorLayout;
    m_extent = other.m_extent;
    m_occlusionFormat = other.m_occlusionFormat;
    m_occlusionImage = std::move(other.m_occlusionImage);
    m_renderPass = other.m_renderPass;
    m_pipelineLayout = other.m_pipelineLayout;
    m_pipeline = other.m_pipeline;
    m_framebuffer = other.m_framebuffer;

    other.m_device = VK_NULL_HANDLE;
    other.m_physicalDevice = VK_NULL_HANDLE;
    other.m_renderPass = VK_NULL_HANDLE;
    other.m_pipelineLayout = VK_NULL_HANDLE;
    other.m_pipeline = VK_NULL_HANDLE;
    other.m_framebuffer = VK_NULL_HANDLE;

    return *this;
}

void SSAOPass::resize(const VulkanContext& context, VkExtent2D extent) {
    if (extent.width == 0U || extent.height == 0U) {
        return;
    }
    if (extent.width == m_extent.width && extent.height == m_extent.height) {
        return;
    }
    recreate_framebuffer(context, extent);
}

void SSAOPass::record(VkCommandBuffer commandBuffer, const SSAODescriptors& descriptors) const {
    VkImageMemoryBarrier toColorAttachment {};
    toColorAttachment.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toColorAttachment.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toColorAttachment.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toColorAttachment.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toColorAttachment.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toColorAttachment.image = m_occlusionImage.image();
    toColorAttachment.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toColorAttachment.subresourceRange.baseMipLevel = 0U;
    toColorAttachment.subresourceRange.levelCount = 1U;
    toColorAttachment.subresourceRange.baseArrayLayer = 0U;
    toColorAttachment.subresourceRange.layerCount = 1U;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0U, 0U, nullptr, 0U, nullptr, 1U, &toColorAttachment);

    VkClearValue clearValue {};
    clearValue.color = {{1.0F, 1.0F, 1.0F, 1.0F}};

    VkRenderPassBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = m_renderPass;
    beginInfo.framebuffer = m_framebuffer;
    beginInfo.renderArea.offset = {0, 0};
    beginInfo.renderArea.extent = m_extent;
    beginInfo.clearValueCount = 1U;
    beginInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport {};
    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = static_cast<float>(m_extent.width);
    viewport.height = static_cast<float>(m_extent.height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;
    vkCmdSetViewport(commandBuffer, 0U, 1U, &viewport);

    VkRect2D scissor {};
    scissor.offset = {0, 0};
    scissor.extent = m_extent;
    vkCmdSetScissor(commandBuffer, 0U, 1U, &scissor);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    const VkDescriptorSet sets[] = {descriptors.descriptor_set()};
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0U, 1U, sets, 0U, nullptr);

    vkCmdDraw(commandBuffer, 3U, 1U, 0U, 0U);

    vkCmdEndRenderPass(commandBuffer);

}

VkImageView SSAOPass::occlusion_view() const noexcept {
    return m_occlusionImage.view();
}

VkFormat SSAOPass::occlusion_format() const noexcept {
    return m_occlusionFormat;
}

void SSAOPass::destroy() noexcept {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }

    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
    if (m_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_device, m_framebuffer, nullptr);
        m_framebuffer = VK_NULL_HANDLE;
    }

    m_occlusionImage = vk::ColorImage {};
}

void SSAOPass::create_static_resources(const VulkanContext& context, VkDescriptorSetLayout descriptorLayout) {
    static_cast<void>(context);
    const VkDevice device = m_device;

    VkAttachmentDescription attachment {};
    attachment.format = m_occlusionFormat;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef {0U, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1U;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dependency {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0U;
    dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1U;
    renderPassInfo.pAttachments = &attachment;
    renderPassInfo.subpassCount = 1U;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1U;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create SSAO render pass"};
    }

    VkPipelineLayoutCreateInfo layoutInfo {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1U;
    layoutInfo.pSetLayouts = &descriptorLayout;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create SSAO pipeline layout"};
    }

    const auto shaderDir = shader_directory();
    const auto vertCode = read_binary_file(shaderDir / "ssao.vert.spv");
    const auto fragCode = read_binary_file(shaderDir / "ssao.frag.spv");

    VkShaderModule vertModule = create_shader_module(device, vertCode);
    VkShaderModule fragModule = create_shader_module(device, fragCode);

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1U;
    viewportState.scissorCount = 1U;

    VkPipelineRasterizationStateCreateInfo rasterization {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0F;

    VkPipelineMultisampleStateCreateInfo multisample {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttachment {};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
    blendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlend {};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1U;
    colorBlend.pAttachments = &blendAttachment;

    const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2U;
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2U;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterization;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0U;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1U, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(device, fragModule, nullptr);
        vkDestroyShaderModule(device, vertModule, nullptr);
        throw std::runtime_error {"Failed to create SSAO pipeline"};
    }

    vkDestroyShaderModule(device, fragModule, nullptr);
    vkDestroyShaderModule(device, vertModule, nullptr);
}

void SSAOPass::recreate_framebuffer(const VulkanContext& context, VkExtent2D extent) {
    static_cast<void>(context);
    if (m_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_device, m_framebuffer, nullptr);
        m_framebuffer = VK_NULL_HANDLE;
    }
    m_occlusionImage = vk::ColorImage::create(m_physicalDevice, m_device, extent, m_occlusionFormat,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    m_extent = extent;

    VkCommandPool commandPool {VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer {VK_NULL_HANDLE};
    try {
        VkCommandPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex = context.graphics_queue_family_index();

        if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create SSAO transition command pool"};
        }

        VkCommandBufferAllocateInfo allocateInfo {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1U;

        if (vkAllocateCommandBuffers(m_device, &allocateInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to allocate SSAO transition command buffer"};
        }

        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to begin SSAO transition commands"};
        }

        VkImageSubresourceRange range {};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0U;
        range.levelCount = 1U;
        range.baseArrayLayer = 0U;
        range.layerCount = 1U;

        VkImageMemoryBarrier toTransfer {};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcAccessMask = 0U;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.image = m_occlusionImage.image();
        toTransfer.subresourceRange = range;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0U, 0U, nullptr, 0U, nullptr, 1U, &toTransfer);

        VkClearColorValue clearValue {{1.0F, 1.0F, 1.0F, 1.0F}};
        vkCmdClearColorImage(commandBuffer, m_occlusionImage.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue,
            1U, &range);

        VkImageMemoryBarrier toShaderRead {};
        toShaderRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toShaderRead.image = m_occlusionImage.image();
        toShaderRead.subresourceRange = range;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0U, 0U, nullptr, 0U, nullptr, 1U, &toShaderRead);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to record SSAO transition commands"};
        }

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1U;
        submitInfo.pCommandBuffers = &commandBuffer;

        const VkQueue queue = context.graphics_queue();
        if (vkQueueSubmit(queue, 1U, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to submit SSAO transition commands"};
        }
        if (vkQueueWaitIdle(queue) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to wait for SSAO transition queue"};
        }
    } catch (...) {
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, commandPool, nullptr);
        }
        throw;
    }
    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, commandPool, nullptr);
    }

    VkImageView attachments[] = {m_occlusionImage.view()};

    VkFramebufferCreateInfo framebufferInfo {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_renderPass;
    framebufferInfo.attachmentCount = 1U;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = 1U;

    if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffer) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create SSAO framebuffer"};
    }
}

SSAOCompositeDescriptors::SSAOCompositeDescriptors(const VulkanContext& context, VkImageView initialOcclusionView)
    : m_device {context.device()} {
    try {
        const VkDevice device = m_device;
        const VkPhysicalDevice physicalDevice = context.physical_device();

        VkDescriptorPoolSize poolSizes[2] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1U},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1U}
        };

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 2U;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = 1U;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create SSAO composite descriptor pool"};
        }

        VkDescriptorSetLayoutBinding bindings[2] = {};
        bindings[0].binding = 0U;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1U;
        bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        bindings[1].binding = 1U;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount = 1U;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 2U;
        layoutInfo.pBindings = bindings;

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create SSAO composite descriptor layout"};
        }

        VkDescriptorSetAllocateInfo allocateInfo {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = m_descriptorPool;
        allocateInfo.descriptorSetCount = 1U;
        allocateInfo.pSetLayouts = &m_layout;

        if (vkAllocateDescriptorSets(device, &allocateInfo, &m_descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to allocate SSAO composite descriptor set"};
        }

        const VkDeviceSize configSize = sizeof(Config);
        m_configBuffer = create_buffer(physicalDevice, device, configSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_configMemory);

        VkSamplerCreateInfo samplerInfo {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &m_occlusionSampler) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create SSAO occlusion sampler"};
        }

        VkDescriptorBufferInfo bufferInfo {};
        bufferInfo.buffer = m_configBuffer;
        bufferInfo.offset = 0U;
        bufferInfo.range = configSize;

        VkWriteDescriptorSet bufferWrite {};
        bufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        bufferWrite.dstSet = m_descriptorSet;
        bufferWrite.dstBinding = 0U;
        bufferWrite.descriptorCount = 1U;
        bufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bufferWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device, 1U, &bufferWrite, 0U, nullptr);

        set_config(1.0F, 0.2F);
        if (initialOcclusionView != VK_NULL_HANDLE) {
            update_occlusion_view(initialOcclusionView);
        }
    } catch (...) {
        destroy();
        throw;
    }
}

SSAOCompositeDescriptors::~SSAOCompositeDescriptors() noexcept {
    destroy();
}

SSAOCompositeDescriptors::SSAOCompositeDescriptors(SSAOCompositeDescriptors&& other) noexcept {
    *this = std::move(other);
}

SSAOCompositeDescriptors& SSAOCompositeDescriptors::operator=(SSAOCompositeDescriptors&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy();

    m_device = other.m_device;
    m_descriptorPool = other.m_descriptorPool;
    m_layout = other.m_layout;
    m_descriptorSet = other.m_descriptorSet;
    m_configBuffer = other.m_configBuffer;
    m_configMemory = other.m_configMemory;
    m_occlusionSampler = other.m_occlusionSampler;
    m_config = other.m_config;
    m_currentOcclusionView = other.m_currentOcclusionView;

    other.m_device = VK_NULL_HANDLE;
    other.m_descriptorPool = VK_NULL_HANDLE;
    other.m_layout = VK_NULL_HANDLE;
    other.m_descriptorSet = VK_NULL_HANDLE;
    other.m_configBuffer = VK_NULL_HANDLE;
    other.m_configMemory = VK_NULL_HANDLE;
    other.m_occlusionSampler = VK_NULL_HANDLE;
    other.m_currentOcclusionView = VK_NULL_HANDLE;

    return *this;
}

void SSAOCompositeDescriptors::update_occlusion_view(VkImageView view) {
    if (view == VK_NULL_HANDLE || m_device == VK_NULL_HANDLE) {
        return;
    }
    m_currentOcclusionView = view;

    VkDescriptorImageInfo imageInfo {};
    imageInfo.sampler = m_occlusionSampler;
    imageInfo.imageView = view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSet;
    write.dstBinding = 1U;
    write.descriptorCount = 1U;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_device, 1U, &write, 0U, nullptr);
}

void SSAOCompositeDescriptors::set_config(float strength, float baseAmbient) {
    m_config.occlusionStrength = strength;
    m_config.baseAmbient = baseAmbient;
    write_config();
}

VkDescriptorSetLayout SSAOCompositeDescriptors::layout() const noexcept {
    return m_layout;
}

VkDescriptorSet SSAOCompositeDescriptors::descriptor_set() const noexcept {
    return m_descriptorSet;
}

void SSAOCompositeDescriptors::write_config() const {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }
    copy_to_memory(m_device, m_configMemory, &m_config, sizeof(Config));
}

void SSAOCompositeDescriptors::destroy() noexcept {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }

    if (m_occlusionSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_occlusionSampler, nullptr);
        m_occlusionSampler = VK_NULL_HANDLE;
    }
    if (m_configBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_configBuffer, nullptr);
        m_configBuffer = VK_NULL_HANDLE;
    }
    if (m_configMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_configMemory, nullptr);
        m_configMemory = VK_NULL_HANDLE;
    }
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }

    m_descriptorSet = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_currentOcclusionView = VK_NULL_HANDLE;
}
} // namespace vulkano::app
