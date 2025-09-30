#include <vulkano/app/texture_image.hpp>

#include <vulkano/app/texture_loader.hpp>
#include <vulkano/app/vulkan_context.hpp>

#include <array>
#include <cstring>
#include <stdexcept>

namespace vulkano::app {
namespace {
[[nodiscard]] VkFormat pick_format(TextureColorSpace colorSpace) noexcept {
    switch (colorSpace) {
    case TextureColorSpace::sRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case TextureColorSpace::Linear:
    default:
        return VK_FORMAT_R8G8B8A8_UNORM;
    }
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

    throw std::runtime_error {"Failed to find suitable memory type for texture image"};
}

[[nodiscard]] VkBuffer create_buffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
    VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer {VK_NULL_HANDLE};
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create texture staging buffer"};
    }

    VkMemoryRequirements requirements {};
    vkGetBufferMemoryRequirements(device, buffer, &requirements);

    VkMemoryAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = find_memory_type(physicalDevice, requirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocateInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        throw std::runtime_error {"Failed to allocate texture staging buffer memory"};
    }

    if (vkBindBufferMemory(device, buffer, memory, 0U) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
        throw std::runtime_error {"Failed to bind texture staging buffer memory"};
    }

    return buffer;
}

void copy_data(VkDevice device, VkDeviceMemory memory, const void* data, VkDeviceSize size) {
    void* mapped = nullptr;
    if (vkMapMemory(device, memory, 0U, size, 0U, &mapped) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to map staging buffer"};
    }
    std::memcpy(mapped, data, static_cast<std::size_t>(size));
    vkUnmapMemory(device, memory);
}

void transition_image(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0U;
    barrier.subresourceRange.levelCount = 1U;
    barrier.subresourceRange.baseArrayLayer = 0U;
    barrier.subresourceRange.layerCount = 1U;

    VkPipelineStageFlags sourceStage {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
    VkPipelineStageFlags destinationStage {VK_PIPELINE_STAGE_TRANSFER_BIT};

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0U;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error {"Unsupported texture layout transition"};
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0U, 0U, nullptr, 0U, nullptr, 1U, &barrier);
}

void submit_and_wait(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkCommandBuffer commandBuffer) {
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to end texture upload command buffer"};
    }

    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1U;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(queue, 1U, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to submit texture upload commands"};
    }
    if (vkQueueWaitIdle(queue) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to wait for texture upload queue"};
    }

    vkFreeCommandBuffers(device, commandPool, 1U, &commandBuffer);
    vkDestroyCommandPool(device, commandPool, nullptr);
}
} // namespace

TextureImage TextureImage::create(const VulkanContext& context, const TextureData& data) {
    if (data.width == 0U || data.height == 0U || data.pixels.empty()) {
        throw std::invalid_argument {"Texture data must be populated"};
    }

    const VkDevice device = context.device();
    const VkPhysicalDevice physicalDevice = context.physical_device();
    const VkQueue queue = context.graphics_queue();
    const std::uint32_t queueFamily = context.graphics_queue_family_index();

    const VkFormat format = pick_format(data.colorSpace);

    VkImage image {VK_NULL_HANDLE};
    VkDeviceMemory imageMemory {VK_NULL_HANDLE};

    VkImageCreateInfo imageInfo {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = data.width;
    imageInfo.extent.height = data.height;
    imageInfo.extent.depth = 1U;
    imageInfo.mipLevels = 1U;
    imageInfo.arrayLayers = 1U;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create texture image"};
    }

    VkMemoryRequirements requirements {};
    vkGetImageMemoryRequirements(device, image, &requirements);

    VkMemoryAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = find_memory_type(physicalDevice, requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocateInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        vkDestroyImage(device, image, nullptr);
        throw std::runtime_error {"Failed to allocate texture image memory"};
    }

    if (vkBindImageMemory(device, image, imageMemory, 0U) != VK_SUCCESS) {
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, imageMemory, nullptr);
        throw std::runtime_error {"Failed to bind texture image memory"};
    }

    VkDeviceMemory stagingMemory {VK_NULL_HANDLE};
    const VkDeviceSize bufferSize {
        static_cast<VkDeviceSize>(data.pixels.size())
    };
    VkBuffer stagingBuffer = create_buffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingMemory);
    copy_data(device, stagingMemory, data.pixels.data(), bufferSize);

    VkCommandPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = queueFamily;

    VkCommandPool commandPool {VK_NULL_HANDLE};
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, imageMemory, nullptr);
        throw std::runtime_error {"Failed to create texture upload command pool"};
    }

    VkCommandBufferAllocateInfo commandAlloc {};
    commandAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandAlloc.commandPool = commandPool;
    commandAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandAlloc.commandBufferCount = 1U;

    VkCommandBuffer commandBuffer {VK_NULL_HANDLE};
    if (vkAllocateCommandBuffers(device, &commandAlloc, &commandBuffer) != VK_SUCCESS) {
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, imageMemory, nullptr);
        throw std::runtime_error {"Failed to allocate texture upload command buffer"};
    }

    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, commandPool, 1U, &commandBuffer);
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, imageMemory, nullptr);
        throw std::runtime_error {"Failed to begin texture upload command buffer"};
    }

    transition_image(commandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion {};
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0U;
    copyRegion.imageSubresource.baseArrayLayer = 0U;
    copyRegion.imageSubresource.layerCount = 1U;
    copyRegion.imageExtent.width = data.width;
    copyRegion.imageExtent.height = data.height;
    copyRegion.imageExtent.depth = 1U;

    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1U, &copyRegion);

    transition_image(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    submit_and_wait(device, queue, commandPool, commandBuffer);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    VkImageViewCreateInfo viewInfo {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0U;
    viewInfo.subresourceRange.levelCount = 1U;
    viewInfo.subresourceRange.baseArrayLayer = 0U;
    viewInfo.subresourceRange.layerCount = 1U;

    VkImageView imageView {VK_NULL_HANDLE};
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, imageMemory, nullptr);
        throw std::runtime_error {"Failed to create texture image view"};
    }

    VkSamplerCreateInfo samplerInfo {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0F;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkSampler sampler {VK_NULL_HANDLE};
    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        vkDestroyImageView(device, imageView, nullptr);
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, imageMemory, nullptr);
        throw std::runtime_error {"Failed to create texture sampler"};
    }

    VkExtent2D extent {data.width, data.height};
    return TextureImage {device, image, imageMemory, imageView, sampler, format, extent};
}

TextureImage::TextureImage(VkDevice device, VkImage image, VkDeviceMemory memory, VkImageView view, VkSampler sampler,
    VkFormat format, VkExtent2D extent) noexcept
    : m_device {device}
    , m_image {image}
    , m_memory {memory}
    , m_view {view}
    , m_sampler {sampler}
    , m_format {format}
    , m_extent {extent} {
}

TextureImage::TextureImage(TextureImage&& other) noexcept {
    *this = std::move(other);
}

TextureImage& TextureImage::operator=(TextureImage&& other) noexcept {
    if (this != &other) {
        destroy();
        m_device = other.m_device;
        m_image = other.m_image;
        m_memory = other.m_memory;
        m_view = other.m_view;
        m_sampler = other.m_sampler;
        m_format = other.m_format;
        m_extent = other.m_extent;

        other.m_device = VK_NULL_HANDLE;
        other.m_image = VK_NULL_HANDLE;
        other.m_memory = VK_NULL_HANDLE;
        other.m_view = VK_NULL_HANDLE;
        other.m_sampler = VK_NULL_HANDLE;
    }
    return *this;
}

TextureImage::~TextureImage() noexcept {
    destroy();
}

VkImage TextureImage::image() const noexcept {
    return m_image;
}

VkImageView TextureImage::view() const noexcept {
    return m_view;
}

VkSampler TextureImage::sampler() const noexcept {
    return m_sampler;
}

VkFormat TextureImage::format() const noexcept {
    return m_format;
}

VkExtent2D TextureImage::extent() const noexcept {
    return m_extent;
}

void TextureImage::destroy() noexcept {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_view != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_view, nullptr);
        m_view = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
    m_device = VK_NULL_HANDLE;
}
} // namespace vulkano::app

