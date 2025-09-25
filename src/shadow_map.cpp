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

ShadowMap::ShadowMap(const VulkanContext& context, std::uint32_t resolution, std::uint32_t layers) {
    initialise(context, resolution, layers);
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

auto ShadowMap::create(const VulkanContext& context, std::uint32_t resolution, std::uint32_t layers) -> ShadowMap {
    return ShadowMap {context, resolution, layers};
}

void ShadowMap::recreate(const VulkanContext& context, std::uint32_t resolution, std::uint32_t layers) {
    cleanup();
    initialise(context, resolution, layers);
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
    return m_layeredView;
}

auto ShadowMap::layered_view() const noexcept -> VkImageView {
    return m_layeredView;
}

auto ShadowMap::layer_views() const noexcept -> const std::vector<VkImageView>& {
    return m_layerViews;
}

auto ShadowMap::layer_count() const noexcept -> std::uint32_t {
    return m_layers;
}

auto ShadowMap::sampler() const noexcept -> VkSampler {
    return m_sampler;
}

void ShadowMap::initialise(const VulkanContext& context, std::uint32_t resolution, std::uint32_t layers) {
    if(resolution == 0U) {
        throw std::invalid_argument {"Shadow map resolution must be greater than zero"};
    }
    if(layers == 0U || layers > maxLayers) {
        throw std::invalid_argument {"Shadow map layer count must be between 1 and maxLayers"};
    }

    m_device = context.device();
    m_resolution = resolution;
    m_layers = layers;
    m_format = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo imageInfo {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = resolution;
    imageInfo.extent.height = resolution;
    imageInfo.extent.depth = 1U;
    imageInfo.mipLevels = 1U;
    imageInfo.arrayLayers = layers;
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
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = m_format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0U;
    viewInfo.subresourceRange.levelCount = 1U;
    viewInfo.subresourceRange.baseArrayLayer = 0U;
    viewInfo.subresourceRange.layerCount = layers;

    if(vkCreateImageView(m_device, &viewInfo, nullptr, &m_layeredView) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create shadow map layered image view"};
    }
    context.set_object_name(
        VK_OBJECT_TYPE_IMAGE_VIEW,
        reinterpret_cast<std::uint64_t>(m_layeredView),
        "Shadow Map Layered View");

    m_layerViews.resize(layers, VK_NULL_HANDLE);
    for(std::uint32_t layer {0U}; layer < layers; ++layer) {
        VkImageViewCreateInfo layerViewInfo = viewInfo;
        layerViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        layerViewInfo.subresourceRange.baseArrayLayer = layer;
        layerViewInfo.subresourceRange.layerCount = 1U;
        if(vkCreateImageView(m_device, &layerViewInfo, nullptr, &m_layerViews.at(layer)) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create shadow map layer view"};
        }
        const std::string name = "Shadow Map Layer View " + std::to_string(layer);
        context.set_object_name(
            VK_OBJECT_TYPE_IMAGE_VIEW,
            reinterpret_cast<std::uint64_t>(m_layerViews.at(layer)),
            name);
    }

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
    for(VkImageView layerView : m_layerViews) {
        if(layerView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, layerView, nullptr);
        }
    }
    m_layerViews.clear();
    if(m_layeredView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_layeredView, nullptr);
    }
    if(m_image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_image, nullptr);
    }
    if(m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
    }

    m_sampler = VK_NULL_HANDLE;
    m_layeredView = VK_NULL_HANDLE;
    m_image = VK_NULL_HANDLE;
    m_memory = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_resolution = 0U;
    m_layers = 0U;
}

void ShadowMap::move_from(ShadowMap&& other) noexcept {
    m_device = other.m_device;
    m_image = other.m_image;
    m_memory = other.m_memory;
    m_layeredView = other.m_layeredView;
    m_sampler = other.m_sampler;
    m_format = other.m_format;
    m_resolution = other.m_resolution;
    m_layers = other.m_layers;
    m_layerViews = std::move(other.m_layerViews);

    other.m_device = VK_NULL_HANDLE;
    other.m_image = VK_NULL_HANDLE;
    other.m_memory = VK_NULL_HANDLE;
    other.m_layeredView = VK_NULL_HANDLE;
    other.m_sampler = VK_NULL_HANDLE;
    other.m_resolution = 0U;
    other.m_layers = 0U;
    other.m_layerViews.clear();
}

} // namespace vulkano
