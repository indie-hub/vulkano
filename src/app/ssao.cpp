#include <vulkano/app/ssao.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

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

SSAODescriptors::SSAODescriptors(const VulkanContext& context, const SSAOGpuResources& resources)
    : m_device {context.device()} {
    try {
        const VkDevice device = m_device;

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
            throw std::runtime_error {"Failed to create SSAO descriptor pool"};
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

        VkDescriptorBufferInfo bufferInfo {};
        bufferInfo.buffer = resources.kernel_buffer();
        bufferInfo.offset = 0U;
        bufferInfo.range = VK_WHOLE_SIZE;

        VkDescriptorImageInfo imageInfo {};
        imageInfo.sampler = resources.noise_sampler();
        imageInfo.imageView = resources.noise_image_view();
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        std::array<VkWriteDescriptorSet, 2> writes {
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
                .pImageInfo = &imageInfo,
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr
            }
        };

        vkUpdateDescriptorSets(device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0U, nullptr);
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

    other.m_device = VK_NULL_HANDLE;
    other.m_descriptorPool = VK_NULL_HANDLE;
    other.m_layout = VK_NULL_HANDLE;
    other.m_descriptorSet = VK_NULL_HANDLE;

    return *this;
}

VkDescriptorSetLayout SSAODescriptors::layout() const noexcept {
    return m_layout;
}

VkDescriptorSet SSAODescriptors::descriptor_set() const noexcept {
    return m_descriptorSet;
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

    m_descriptorSet = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
}
} // namespace vulkano::app
