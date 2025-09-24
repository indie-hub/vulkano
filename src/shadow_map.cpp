#include <vulkano/shadow_map.hpp>

#include <stdexcept>
#include <string>

namespace {
    constexpr VkImageUsageFlags shadowImageUsage {
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    };

    auto find_memory_type(
        VkPhysicalDevice physicalDevice,
        std::uint32_t typeFilter,
        VkMemoryPropertyFlags properties) -> std::uint32_t {
        VkPhysicalDeviceMemoryProperties memoryProperties {};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

        for(std::uint32_t index {0U}; index < memoryProperties.memoryTypeCount; ++index) {
            const bool suitable = (typeFilter & (1U << index)) != 0U;
            const bool hasProperties = (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties;
            if(suitable && hasProperties) {
                return index;
            }
        }
        throw std::runtime_error {"Failed to find suitable memory type for shadow map"};
    }
}

namespace vulkano {

ShadowMap::ShadowMap(const VulkanContext& context, std::uint32_t resolution) {
    initialise(context, resolution);
}

ShadowMap::ShadowMap(ShadowMap&& other) noexcept {
    move_from(std::move(other));
}

auto ShadowMap::operator=(ShadowMap&& other) noexcept -> ShadowMap& {
    if(this != &other) {
        cleanup();
        move_from(std::move(other));
    }
    return *this;
}

ShadowMap::~ShadowMap() noexcept {
    cleanup();
}

auto ShadowMap::create(const VulkanContext& context, std::uint32_t resolution) -> ShadowMap {
    return ShadowMap {context, resolution};
}

void ShadowMap::recreate(const VulkanContext& context, std::uint32_t resolution) {
    cleanup();
    initialise(context, resolution);
}

auto ShadowMap::resolution() const noexcept -> std::uint32_t {
    return m_resolution;
}

auto ShadowMap::format() const noexcept -> VkFormat {
    return m_format;
}

auto ShadowMap::image() const noexcept -> VkImage {
    return m_image;
}

auto ShadowMap::image_view() const noexcept -> VkImageView {
    return m_imageView;
}

auto ShadowMap::sampler() const noexcept -> VkSampler {
    return m_sampler;
}

void ShadowMap::initialise(const VulkanContext& context, std::uint32_t resolution) {
    if(resolution == 0U) {
        throw std::invalid_argument {"Shadow map resolution must be greater than zero"};
    }

    m_device = context.device();
    m_resolution = resolution;
    m_format = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo imageInfo {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = resolution;
    imageInfo.extent.height = resolution;
    imageInfo.extent.depth = 1U;
    imageInfo.mipLevels = 1U;
    imageInfo.arrayLayers = 1U;
    imageInfo.format = m_format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = shadowImageUsage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if(vkCreateImage(m_device, &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create shadow map image"};
    }
    context.set_object_name(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<std::uint64_t>(m_image), "Shadow Map Image");

    VkMemoryRequirements requirements {};
    vkGetImageMemoryRequirements(m_device, m_image, &requirements);

    VkMemoryAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = find_memory_type(
        context.physical_device(),
        requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if(vkAllocateMemory(m_device, &allocateInfo, nullptr, &m_memory) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to allocate shadow map image memory"};
    }
    context.set_object_name(VK_OBJECT_TYPE_DEVICE_MEMORY, reinterpret_cast<std::uint64_t>(m_memory), "Shadow Map Memory");

    if(vkBindImageMemory(m_device, m_image, m_memory, 0U) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to bind shadow map image memory"};
    }

    VkImageViewCreateInfo viewInfo {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0U;
    viewInfo.subresourceRange.levelCount = 1U;
    viewInfo.subresourceRange.baseArrayLayer = 0U;
    viewInfo.subresourceRange.layerCount = 1U;

    if(vkCreateImageView(m_device, &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create shadow map image view"};
    }
    context.set_object_name(
        VK_OBJECT_TYPE_IMAGE_VIEW,
        reinterpret_cast<std::uint64_t>(m_imageView),
        "Shadow Map Image View");

    VkSamplerCreateInfo samplerInfo {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.mipLodBias = 0.0F;
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    samplerInfo.minLod = 0.0F;
    samplerInfo.maxLod = 1.0F;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    if(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create shadow map sampler"};
    }
    context.set_object_name(
        VK_OBJECT_TYPE_SAMPLER,
        reinterpret_cast<std::uint64_t>(m_sampler),
        "Shadow Map Sampler");
}

void ShadowMap::cleanup() noexcept {
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
    m_resolution = 0U;
}

void ShadowMap::move_from(ShadowMap&& other) noexcept {
    m_device = other.m_device;
    m_image = other.m_image;
    m_memory = other.m_memory;
    m_imageView = other.m_imageView;
    m_sampler = other.m_sampler;
    m_format = other.m_format;
    m_resolution = other.m_resolution;

    other.m_device = VK_NULL_HANDLE;
    other.m_image = VK_NULL_HANDLE;
    other.m_memory = VK_NULL_HANDLE;
    other.m_imageView = VK_NULL_HANDLE;
    other.m_sampler = VK_NULL_HANDLE;
    other.m_resolution = 0U;
}

} // namespace vulkano

