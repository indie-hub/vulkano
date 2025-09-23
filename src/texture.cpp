#include <vulkano/texture.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
    constexpr VkSampleCountFlagBits textureSampleCount {VK_SAMPLE_COUNT_1_BIT};
    constexpr VkImageTiling defaultTiling {VK_IMAGE_TILING_OPTIMAL};
    constexpr VkSharingMode defaultSharing {VK_SHARING_MODE_EXCLUSIVE};
    constexpr VkImageType textureImageType {VK_IMAGE_TYPE_2D};
    constexpr std::uint32_t textureArrayLayers {1U};
    constexpr std::uint32_t textureQueueFamilyIgnored {VK_QUEUE_FAMILY_IGNORED};

    auto clamp_dimension(std::uint32_t value) -> std::uint32_t {
        return std::max(value, 1U);
    }

    auto lod_clamp() -> float {
        return VK_LOD_CLAMP_NONE;
    }
}

namespace vulkano {

TextureImage::TextureImage(
    const VulkanContext& context,
    VkExtent2D extent,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageAspectFlags aspectMask,
    std::uint32_t mipLevels,
    std::string_view name)
    : m_device {context.device()}
    , m_image {VK_NULL_HANDLE}
    , m_memory {VK_NULL_HANDLE}
    , m_imageView {VK_NULL_HANDLE}
    , m_extent {extent}
    , m_format {format}
    , m_aspectMask {aspectMask}
    , m_mipLevels {std::max(mipLevels, 1U)} {
    if(m_device == VK_NULL_HANDLE) {
        throw std::runtime_error {"TextureImage requires a valid Vulkan device"};
    }
    if(extent.width == 0U || extent.height == 0U) {
        throw std::invalid_argument {"TextureImage extent must be greater than zero"};
    }
    create_image(context, extent, format, usage, m_mipLevels, m_image, m_memory);
    create_image_view(context, m_image, format, aspectMask, m_mipLevels, m_imageView);

    const std::string baseName {name};
    context.set_object_name(
        VK_OBJECT_TYPE_IMAGE,
        reinterpret_cast<std::uint64_t>(m_image),
        baseName + " Image");
    context.set_object_name(
        VK_OBJECT_TYPE_DEVICE_MEMORY,
        reinterpret_cast<std::uint64_t>(m_memory),
        baseName + " Image Memory");
    context.set_object_name(
        VK_OBJECT_TYPE_IMAGE_VIEW,
        reinterpret_cast<std::uint64_t>(m_imageView),
        baseName + " Image View");
}

TextureImage::TextureImage(TextureImage&& other) noexcept {
    move_from(std::move(other));
}

auto TextureImage::operator=(TextureImage&& other) noexcept -> TextureImage& {
    if(this != &other) {
        cleanup();
        move_from(std::move(other));
    }
    return *this;
}

TextureImage::~TextureImage() noexcept {
    cleanup();
}

auto TextureImage::create(
    const VulkanContext& context,
    VkExtent2D extent,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageAspectFlags aspectMask,
    std::uint32_t mipLevels,
    std::string_view name) -> TextureImage {
    return TextureImage {context, extent, format, usage, aspectMask, mipLevels, name};
}

void TextureImage::transition_layout(
    const VulkanContext& context,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    std::uint32_t baseMipLevel,
    std::uint32_t mipLevelCount) const {
    if(m_image == VK_NULL_HANDLE) {
        throw std::runtime_error {"Cannot transition layout for null image"};
    }
    VkCommandPool commandPool {VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer = begin_single_use_commands(context, commandPool);

    VkImageMemoryBarrier barrier {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = textureQueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = textureQueueFamilyIgnored;
    barrier.image = m_image;
    barrier.subresourceRange.aspectMask = m_aspectMask;
    barrier.subresourceRange.baseMipLevel = baseMipLevel;
    barrier.subresourceRange.levelCount = mipLevelCount;
    barrier.subresourceRange.baseArrayLayer = 0U;
    barrier.subresourceRange.layerCount = textureArrayLayers;

    VkPipelineStageFlags sourceStage {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
    VkPipelineStageFlags destinationStage {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};

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
    } else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = 0U;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        end_single_use_commands(context, commandPool, commandBuffer);
        throw std::invalid_argument {"Unsupported image layout transition"};
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

    end_single_use_commands(context, commandPool, commandBuffer);
}

void TextureImage::copy_buffer_to_image(
    const VulkanContext& context,
    VkBuffer buffer,
    VkExtent3D extent,
    std::uint32_t baseMipLevel) const {
    if(m_image == VK_NULL_HANDLE) {
        throw std::runtime_error {"Cannot copy to null image"};
    }
    VkCommandPool commandPool {VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer = begin_single_use_commands(context, commandPool);

    VkBufferImageCopy region {};
    region.bufferOffset = 0U;
    region.bufferRowLength = 0U;
    region.bufferImageHeight = 0U;
    region.imageSubresource.aspectMask = m_aspectMask;
    region.imageSubresource.mipLevel = baseMipLevel;
    region.imageSubresource.baseArrayLayer = 0U;
    region.imageSubresource.layerCount = textureArrayLayers;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = extent;

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        m_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1U,
        &region);

    end_single_use_commands(context, commandPool, commandBuffer);
}

void TextureImage::generate_mipmaps(const VulkanContext& context) const {
    if(m_image == VK_NULL_HANDLE) {
        throw std::runtime_error {"Cannot generate mipmaps for null image"};
    }
    if(m_mipLevels <= 1U) {
        transition_layout(
            context,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0U,
            1U);
        return;
    }

    VkFormatProperties properties {};
    vkGetPhysicalDeviceFormatProperties(context.physical_device(), m_format, &properties);
    const bool supportsLinear = (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0U;
    if(!supportsLinear) {
        throw std::runtime_error {"Image format does not support linear blitting for mipmaps"};
    }

    VkCommandPool commandPool {VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer = begin_single_use_commands(context, commandPool);

    VkImageMemoryBarrier barrier {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = m_image;
    barrier.srcQueueFamilyIndex = textureQueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = textureQueueFamilyIgnored;
    barrier.subresourceRange.aspectMask = m_aspectMask;
    barrier.subresourceRange.baseArrayLayer = 0U;
    barrier.subresourceRange.layerCount = textureArrayLayers;
    barrier.subresourceRange.levelCount = 1U;

    std::uint32_t mipWidth = m_extent.width;
    std::uint32_t mipHeight = m_extent.height;

    for(std::uint32_t level {1U}; level < m_mipLevels; ++level) {
        const std::uint32_t previousLevel = level - 1U;

        barrier.subresourceRange.baseMipLevel = previousLevel;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0U,
            0U,
            nullptr,
            0U,
            nullptr,
            1U,
            &barrier);

        VkImageBlit blit {};
        blit.srcSubresource.aspectMask = m_aspectMask;
        blit.srcSubresource.mipLevel = previousLevel;
        blit.srcSubresource.baseArrayLayer = 0U;
        blit.srcSubresource.layerCount = textureArrayLayers;
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {
            static_cast<int32_t>(clamp_dimension(mipWidth)),
            static_cast<int32_t>(clamp_dimension(mipHeight)),
            1
        };

        const std::uint32_t nextWidth = clamp_dimension(mipWidth / 2U);
        const std::uint32_t nextHeight = clamp_dimension(mipHeight / 2U);

        blit.dstSubresource.aspectMask = m_aspectMask;
        blit.dstSubresource.mipLevel = level;
        blit.dstSubresource.baseArrayLayer = 0U;
        blit.dstSubresource.layerCount = textureArrayLayers;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {
            static_cast<int32_t>(nextWidth),
            static_cast<int32_t>(nextHeight),
            1
        };

        vkCmdBlitImage(
            commandBuffer,
            m_image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1U,
            &blit,
            VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0U,
            0U,
            nullptr,
            0U,
            nullptr,
            1U,
            &barrier);

        mipWidth = nextWidth;
        mipHeight = nextHeight;
    }

    barrier.subresourceRange.baseMipLevel = m_mipLevels - 1U;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0U,
        0U,
        nullptr,
        0U,
        nullptr,
        1U,
        &barrier);

    end_single_use_commands(context, commandPool, commandBuffer);
}

auto TextureImage::image() const noexcept -> VkImage {
    return m_image;
}

auto TextureImage::image_view() const noexcept -> VkImageView {
    return m_imageView;
}

auto TextureImage::mip_levels() const noexcept -> std::uint32_t {
    return m_mipLevels;
}

auto TextureImage::extent() const noexcept -> VkExtent2D {
    return m_extent;
}

auto TextureImage::format() const noexcept -> VkFormat {
    return m_format;
}

void TextureImage::cleanup() noexcept {
    if(m_device != VK_NULL_HANDLE && m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_imageView, nullptr);
    }
    if(m_device != VK_NULL_HANDLE && m_image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_image, nullptr);
    }
    if(m_device != VK_NULL_HANDLE && m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
    }
    m_imageView = VK_NULL_HANDLE;
    m_image = VK_NULL_HANDLE;
    m_memory = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_extent = VkExtent2D {0U, 0U};
    m_format = VK_FORMAT_UNDEFINED;
    m_aspectMask = 0U;
    m_mipLevels = 1U;
}

void TextureImage::move_from(TextureImage&& other) noexcept {
    m_device = other.m_device;
    m_image = other.m_image;
    m_memory = other.m_memory;
    m_imageView = other.m_imageView;
    m_extent = other.m_extent;
    m_format = other.m_format;
    m_aspectMask = other.m_aspectMask;
    m_mipLevels = other.m_mipLevels;

    other.m_device = VK_NULL_HANDLE;
    other.m_image = VK_NULL_HANDLE;
    other.m_memory = VK_NULL_HANDLE;
    other.m_imageView = VK_NULL_HANDLE;
    other.m_extent = VkExtent2D {0U, 0U};
    other.m_format = VK_FORMAT_UNDEFINED;
    other.m_aspectMask = 0U;
    other.m_mipLevels = 1U;
}

void TextureImage::create_image(
    const VulkanContext& context,
    VkExtent2D extent,
    VkFormat format,
    VkImageUsageFlags usage,
    std::uint32_t mipLevels,
    VkImage& image,
    VkDeviceMemory& memory) {
    VkImageCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.imageType = textureImageType;
    createInfo.extent = {extent.width, extent.height, 1U};
    createInfo.mipLevels = mipLevels;
    createInfo.arrayLayers = textureArrayLayers;
    createInfo.format = format;
    createInfo.tiling = defaultTiling;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage = usage;
    createInfo.samples = textureSampleCount;
    createInfo.sharingMode = defaultSharing;

    if(vkCreateImage(context.device(), &createInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create texture image"};
    }

    VkMemoryRequirements requirements {};
    vkGetImageMemoryRequirements(context.device(), image, &requirements);

    VkMemoryAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(
        context,
        requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if(vkAllocateMemory(context.device(), &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyImage(context.device(), image, nullptr);
        throw std::runtime_error {"Failed to allocate texture image memory"};
    }

    if(vkBindImageMemory(context.device(), image, memory, 0U) != VK_SUCCESS) {
        vkFreeMemory(context.device(), memory, nullptr);
        vkDestroyImage(context.device(), image, nullptr);
        throw std::runtime_error {"Failed to bind texture image memory"};
    }
}

void TextureImage::create_image_view(
    const VulkanContext& context,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspectMask,
    std::uint32_t mipLevels,
    VkImageView& imageView) {
    VkImageViewCreateInfo viewInfo {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectMask;
    viewInfo.subresourceRange.baseMipLevel = 0U;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0U;
    viewInfo.subresourceRange.layerCount = textureArrayLayers;

    if(vkCreateImageView(context.device(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create texture image view"};
    }
}

auto TextureImage::find_memory_type(
    const VulkanContext& context,
    std::uint32_t typeFilter,
    VkMemoryPropertyFlags properties) -> std::uint32_t {
    VkPhysicalDeviceMemoryProperties memoryProperties {};
    vkGetPhysicalDeviceMemoryProperties(context.physical_device(), &memoryProperties);

    for(std::uint32_t index {0U}; index < memoryProperties.memoryTypeCount; ++index) {
        const bool supported = (typeFilter & (1U << index)) != 0U;
        const bool hasProperties = (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties;
        if(supported && hasProperties) {
            return index;
        }
    }
    throw std::runtime_error {"Failed to find suitable memory type for texture"};
}

auto TextureImage::begin_single_use_commands(const VulkanContext& context, VkCommandPool& commandPool)
    -> VkCommandBuffer {
    VkCommandPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = context.graphics_queue_index();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    if(vkCreateCommandPool(context.device(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create command pool for texture operation"};
    }

    VkCommandBufferAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1U;

    VkCommandBuffer commandBuffer {VK_NULL_HANDLE};
    if(vkAllocateCommandBuffers(context.device(), &allocateInfo, &commandBuffer) != VK_SUCCESS) {
        vkDestroyCommandPool(context.device(), commandPool, nullptr);
        throw std::runtime_error {"Failed to allocate command buffer for texture operation"};
    }

    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(context.device(), commandPool, 1U, &commandBuffer);
        vkDestroyCommandPool(context.device(), commandPool, nullptr);
        throw std::runtime_error {"Failed to begin command buffer for texture operation"};
    }
    return commandBuffer;
}

void TextureImage::end_single_use_commands(
    const VulkanContext& context,
    VkCommandPool commandPool,
    VkCommandBuffer commandBuffer) {
    if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        vkFreeCommandBuffers(context.device(), commandPool, 1U, &commandBuffer);
        vkDestroyCommandPool(context.device(), commandPool, nullptr);
        throw std::runtime_error {"Failed to record texture command buffer"};
    }

    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1U;
    submitInfo.pCommandBuffers = &commandBuffer;

    if(vkQueueSubmit(context.graphics_queue(), 1U, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkFreeCommandBuffers(context.device(), commandPool, 1U, &commandBuffer);
        vkDestroyCommandPool(context.device(), commandPool, nullptr);
        throw std::runtime_error {"Failed to submit texture command buffer"};
    }

    if(vkQueueWaitIdle(context.graphics_queue()) != VK_SUCCESS) {
        vkFreeCommandBuffers(context.device(), commandPool, 1U, &commandBuffer);
        vkDestroyCommandPool(context.device(), commandPool, nullptr);
        throw std::runtime_error {"Failed to wait for queue idle after texture command"};
    }

    vkFreeCommandBuffers(context.device(), commandPool, 1U, &commandBuffer);
    vkDestroyCommandPool(context.device(), commandPool, nullptr);
}

TextureSamplers::TextureSamplers(const VulkanContext& context) {
    initialise(context);
}

TextureSamplers::TextureSamplers(TextureSamplers&& other) noexcept {
    move_from(std::move(other));
}

auto TextureSamplers::operator=(TextureSamplers&& other) noexcept -> TextureSamplers& {
    if(this != &other) {
        cleanup();
        move_from(std::move(other));
    }
    return *this;
}

TextureSamplers::~TextureSamplers() noexcept {
    cleanup();
}

auto TextureSamplers::create(const VulkanContext& context) -> TextureSamplers {
    return TextureSamplers {context};
}

auto TextureSamplers::albedo() const noexcept -> VkSampler {
    return m_albedo;
}

auto TextureSamplers::normal() const noexcept -> VkSampler {
    return m_normal;
}

auto TextureSamplers::anisotropy_enabled() const noexcept -> bool {
    return m_anisotropy;
}

auto TextureSamplers::max_anisotropy() const noexcept -> float {
    return m_maxAnisotropy;
}

void TextureSamplers::initialise(const VulkanContext& context) {
    m_device = context.device();
    if(m_device == VK_NULL_HANDLE) {
        throw std::runtime_error {"TextureSamplers requires a valid Vulkan device"};
    }

    m_anisotropy = context.supports_sampler_anisotropy();
    const VkPhysicalDeviceProperties properties = context.device_properties();
    m_maxAnisotropy = m_anisotropy ? properties.limits.maxSamplerAnisotropy : 1.0F;
    if(m_maxAnisotropy < 1.0F) {
        m_maxAnisotropy = 1.0F;
    }

    VkSamplerCreateInfo samplerInfo {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = m_anisotropy ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = m_anisotropy ? m_maxAnisotropy : 1.0F;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0F;
    samplerInfo.minLod = 0.0F;
    samplerInfo.maxLod = lod_clamp();

    if(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_albedo) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create albedo sampler"};
    }

    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0F;

    if(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_normal) != VK_SUCCESS) {
        vkDestroySampler(m_device, m_albedo, nullptr);
        m_albedo = VK_NULL_HANDLE;
        throw std::runtime_error {"Failed to create normal sampler"};
    }

    context.set_object_name(
        VK_OBJECT_TYPE_SAMPLER,
        reinterpret_cast<std::uint64_t>(m_albedo),
        "Albedo Texture Sampler");
    context.set_object_name(
        VK_OBJECT_TYPE_SAMPLER,
        reinterpret_cast<std::uint64_t>(m_normal),
        "Normal Texture Sampler");
}

void TextureSamplers::cleanup() noexcept {
    if(m_device != VK_NULL_HANDLE && m_albedo != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_albedo, nullptr);
    }
    if(m_device != VK_NULL_HANDLE && m_normal != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_normal, nullptr);
    }
    m_albedo = VK_NULL_HANDLE;
    m_normal = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_anisotropy = false;
    m_maxAnisotropy = 1.0F;
}

void TextureSamplers::move_from(TextureSamplers&& other) noexcept {
    m_device = other.m_device;
    m_albedo = other.m_albedo;
    m_normal = other.m_normal;
    m_anisotropy = other.m_anisotropy;
    m_maxAnisotropy = other.m_maxAnisotropy;

    other.m_device = VK_NULL_HANDLE;
    other.m_albedo = VK_NULL_HANDLE;
    other.m_normal = VK_NULL_HANDLE;
    other.m_anisotropy = false;
    other.m_maxAnisotropy = 1.0F;
}

} // namespace vulkano
