#include <vulkano/texture.hpp>

#include <cstring>
#include <stdexcept>

namespace {
    constexpr VkImageAspectFlags defaultAspectMask {VK_IMAGE_ASPECT_COLOR_BIT};

    auto begin_single_time_commands(const vulkano::VulkanContext& context, VkCommandPool& commandPool) -> VkCommandBuffer {
        VkCommandPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex = context.graphics_queue_index();

        if(vkCreateCommandPool(context.device(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create command pool for texture upload"};
        }

        VkCommandBufferAllocateInfo allocateInfo {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1U;

        VkCommandBuffer commandBuffer {VK_NULL_HANDLE};
        if(vkAllocateCommandBuffers(context.device(), &allocateInfo, &commandBuffer) != VK_SUCCESS) {
            vkDestroyCommandPool(context.device(), commandPool, nullptr);
            throw std::runtime_error {"Failed to allocate command buffer for texture upload"};
        }

        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            vkFreeCommandBuffers(context.device(), commandPool, 1U, &commandBuffer);
            vkDestroyCommandPool(context.device(), commandPool, nullptr);
            throw std::runtime_error {"Failed to begin command buffer for texture upload"};
        }
        return commandBuffer;
    }

    void end_single_time_commands(
        const vulkano::VulkanContext& context,
        VkCommandPool commandPool,
        VkCommandBuffer commandBuffer) {
        if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            vkFreeCommandBuffers(context.device(), commandPool, 1U, &commandBuffer);
            vkDestroyCommandPool(context.device(), commandPool, nullptr);
            throw std::runtime_error {"Failed to record texture upload commands"};
        }

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1U;
        submitInfo.pCommandBuffers = &commandBuffer;

        const VkQueue queue = context.graphics_queue();
        if(vkQueueSubmit(queue, 1U, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            vkFreeCommandBuffers(context.device(), commandPool, 1U, &commandBuffer);
            vkDestroyCommandPool(context.device(), commandPool, nullptr);
            throw std::runtime_error {"Failed to submit texture upload commands"};
        }

        if(vkQueueWaitIdle(queue) != VK_SUCCESS) {
            vkFreeCommandBuffers(context.device(), commandPool, 1U, &commandBuffer);
            vkDestroyCommandPool(context.device(), commandPool, nullptr);
            throw std::runtime_error {"Failed to wait for texture upload completion"};
        }

        vkFreeCommandBuffers(context.device(), commandPool, 1U, &commandBuffer);
        vkDestroyCommandPool(context.device(), commandPool, nullptr);
    }

    void transition_image_layout(
        VkCommandBuffer commandBuffer,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout) {
        VkImageMemoryBarrier barrier {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = defaultAspectMask;
        barrier.subresourceRange.baseMipLevel = 0U;
        barrier.subresourceRange.levelCount = 1U;
        barrier.subresourceRange.baseArrayLayer = 0U;
        barrier.subresourceRange.layerCount = 1U;

        VkPipelineStageFlags sourceStage {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
        VkPipelineStageFlags destinationStage {VK_PIPELINE_STAGE_TRANSFER_BIT};

        if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0U;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            throw std::runtime_error {"Unsupported texture layout transition"};
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage,
            destinationStage,
            0U,
            0U,
            nullptr,
            0U,
            nullptr,
            1U,
            &barrier);
    }

    void copy_buffer_to_image(
        VkCommandBuffer commandBuffer,
        VkBuffer buffer,
        VkImage image,
        VkExtent2D extent) {
        VkBufferImageCopy region {};
        region.bufferOffset = 0U;
        region.bufferRowLength = 0U;
        region.bufferImageHeight = 0U;
        region.imageSubresource.aspectMask = defaultAspectMask;
        region.imageSubresource.mipLevel = 0U;
        region.imageSubresource.baseArrayLayer = 0U;
        region.imageSubresource.layerCount = 1U;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {extent.width, extent.height, 1U};

        vkCmdCopyBufferToImage(
            commandBuffer,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1U,
            &region);
    }
}

namespace vulkano {

Texture2D::Texture2D(
    const VulkanContext& context,
    VkExtent2D extent,
    VkFormat format,
    std::span<const std::uint8_t> pixels,
    VkFilter filter) {
    initialise_image(context, extent, format);
    allocate_memory(context);
    create_image_view(context);
    create_sampler(context, filter);
    upload_pixels(context, pixels);
}

Texture2D::Texture2D(Texture2D&& other) noexcept {
    move_from(std::move(other));
}

auto Texture2D::operator=(Texture2D&& other) noexcept -> Texture2D& {
    if(this != &other) {
        destroy();
        move_from(std::move(other));
    }
    return *this;
}

Texture2D::~Texture2D() noexcept {
    destroy();
}

auto Texture2D::create_rgba8(
    const VulkanContext& context,
    VkExtent2D extent,
    std::span<const std::uint8_t> pixels,
    VkFormat format,
    VkFilter filter) -> Texture2D {
    if(extent.width == 0U || extent.height == 0U) {
        throw std::invalid_argument {"Texture extent must be greater than zero"};
    }
    const std::size_t expectedComponents {4U};
    const std::size_t expectedSize = static_cast<std::size_t>(extent.width) * static_cast<std::size_t>(extent.height) * expectedComponents;
    if(pixels.size() != expectedSize) {
        throw std::invalid_argument {"Texture pixel data size does not match extent"};
    }
    return Texture2D {context, extent, format, pixels, filter};
}

auto Texture2D::image() const noexcept -> VkImage {
    return m_image;
}

auto Texture2D::image_view() const noexcept -> VkImageView {
    return m_imageView;
}

auto Texture2D::sampler() const noexcept -> VkSampler {
    return m_sampler;
}

auto Texture2D::extent() const noexcept -> VkExtent2D {
    return m_extent;
}

auto Texture2D::format() const noexcept -> VkFormat {
    return m_format;
}

void Texture2D::initialise_image(const VulkanContext& context, VkExtent2D extent, VkFormat format) {
    m_device = context.device();
    m_extent = extent;
    m_format = format;

    VkImageCreateInfo imageInfo {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {extent.width, extent.height, 1U};
    imageInfo.mipLevels = 1U;
    imageInfo.arrayLayers = 1U;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if(vkCreateImage(m_device, &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create texture image"};
    }

    context.set_object_name(
        VK_OBJECT_TYPE_IMAGE,
        reinterpret_cast<std::uint64_t>(m_image),
        "Texture Image");
}

void Texture2D::allocate_memory(const VulkanContext& context) {
    VkMemoryRequirements requirements {};
    vkGetImageMemoryRequirements(m_device, m_image, &requirements);

    VkMemoryAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(
        context,
        requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if(vkAllocateMemory(m_device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to allocate texture memory"};
    }

    if(vkBindImageMemory(m_device, m_image, m_memory, 0U) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to bind texture memory"};
    }
}

void Texture2D::create_image_view(const VulkanContext& context) {
    VkImageViewCreateInfo viewInfo {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_format;
    viewInfo.subresourceRange.aspectMask = defaultAspectMask;
    viewInfo.subresourceRange.baseMipLevel = 0U;
    viewInfo.subresourceRange.levelCount = 1U;
    viewInfo.subresourceRange.baseArrayLayer = 0U;
    viewInfo.subresourceRange.layerCount = 1U;

    if(vkCreateImageView(m_device, &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create texture image view"};
    }

    context.set_object_name(
        VK_OBJECT_TYPE_IMAGE_VIEW,
        reinterpret_cast<std::uint64_t>(m_imageView),
        "Texture Image View");
}

void Texture2D::create_sampler(const VulkanContext& context, VkFilter filter) {
    VkSamplerCreateInfo samplerInfo {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = filter;
    samplerInfo.minFilter = filter;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0F;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0F;
    samplerInfo.minLod = 0.0F;
    samplerInfo.maxLod = 0.0F;

    if(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create texture sampler"};
    }

    context.set_object_name(
        VK_OBJECT_TYPE_SAMPLER,
        reinterpret_cast<std::uint64_t>(m_sampler),
        "Texture Sampler");
}

void Texture2D::upload_pixels(const VulkanContext& context, std::span<const std::uint8_t> pixels) {
    if(pixels.empty()) {
        return;
    }

    const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(pixels.size());

    VkBuffer stagingBuffer {VK_NULL_HANDLE};
    VkDeviceMemory stagingMemory {VK_NULL_HANDLE};

    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if(vkCreateBuffer(m_device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create staging buffer for texture upload"};
    }

    VkMemoryRequirements requirements {};
    vkGetBufferMemoryRequirements(m_device, stagingBuffer, &requirements);

    VkMemoryAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(
        context,
        requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if(vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        throw std::runtime_error {"Failed to allocate staging buffer memory"};
    }

    if(vkBindBufferMemory(m_device, stagingBuffer, stagingMemory, 0U) != VK_SUCCESS) {
        vkFreeMemory(m_device, stagingMemory, nullptr);
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        throw std::runtime_error {"Failed to bind staging buffer memory"};
    }

    void* mappedMemory {nullptr};
    if(vkMapMemory(m_device, stagingMemory, 0U, bufferSize, 0U, &mappedMemory) != VK_SUCCESS) {
        vkFreeMemory(m_device, stagingMemory, nullptr);
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        throw std::runtime_error {"Failed to map staging buffer"};
    }
    std::memcpy(mappedMemory, pixels.data(), pixels.size());
    vkUnmapMemory(m_device, stagingMemory);

    VkCommandPool commandPool {VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer = begin_single_time_commands(context, commandPool);
    transition_image_layout(commandBuffer, m_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(commandBuffer, stagingBuffer, m_image, m_extent);
    transition_image_layout(commandBuffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    end_single_time_commands(context, commandPool, commandBuffer);

    vkFreeMemory(m_device, stagingMemory, nullptr);
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
}

void Texture2D::destroy() noexcept {
    if(m_device == VK_NULL_HANDLE) {
        return;
    }
    if(m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sampler, nullptr);
    }
    if(m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_imageView, nullptr);
    }
    if(m_image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_image, nullptr);
    }
    if(m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
    }
    m_sampler = VK_NULL_HANDLE;
    m_imageView = VK_NULL_HANDLE;
    m_image = VK_NULL_HANDLE;
    m_memory = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_extent = {0U, 0U};
    m_format = VK_FORMAT_UNDEFINED;
}

void Texture2D::move_from(Texture2D&& other) noexcept {
    m_device = other.m_device;
    m_image = other.m_image;
    m_memory = other.m_memory;
    m_imageView = other.m_imageView;
    m_sampler = other.m_sampler;
    m_extent = other.m_extent;
    m_format = other.m_format;

    other.m_device = VK_NULL_HANDLE;
    other.m_image = VK_NULL_HANDLE;
    other.m_memory = VK_NULL_HANDLE;
    other.m_imageView = VK_NULL_HANDLE;
    other.m_sampler = VK_NULL_HANDLE;
    other.m_extent = {0U, 0U};
    other.m_format = VK_FORMAT_UNDEFINED;
}

auto Texture2D::find_memory_type(
    const VulkanContext& context,
    std::uint32_t typeFilter,
    VkMemoryPropertyFlags properties) -> std::uint32_t {
    VkPhysicalDeviceMemoryProperties memoryProperties {};
    vkGetPhysicalDeviceMemoryProperties(context.physical_device(), &memoryProperties);

    for(std::uint32_t index {0U}; index < memoryProperties.memoryTypeCount; ++index) {
        const bool suitable = (typeFilter & (1U << index)) != 0U;
        const bool hasProperties = (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties;
        if(suitable && hasProperties) {
            return index;
        }
    }
    throw std::runtime_error {"Failed to find suitable memory type for texture"};
}

} // namespace vulkano

