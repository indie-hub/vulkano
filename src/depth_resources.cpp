#include <vulkano/depth_resources.hpp>

#include <array>
#include <stdexcept>
#include <string>

namespace {
    constexpr std::array<VkFormat, 3U> candidateDepthFormats {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    auto has_stencil_component(VkFormat format) -> bool {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

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
        throw std::runtime_error {"Failed to find suitable memory type for depth resources"};
    }
}

namespace vulkano {

DepthResources::DepthResources(const VulkanContext& context, VkFormat format, VkExtent2D extent, std::uint32_t imageCount) {
    initialise(context, format, extent, imageCount);
}

DepthResources::DepthResources(DepthResources&& other) noexcept {
    move_from(std::move(other));
}

auto DepthResources::operator=(DepthResources&& other) noexcept -> DepthResources& {
    if(this != &other) {
        cleanup();
        move_from(std::move(other));
    }
    return *this;
}

DepthResources::~DepthResources() noexcept {
    cleanup();
}

auto DepthResources::find_supported_format(const VulkanContext& context) -> VkFormat {
    const VkPhysicalDevice physicalDevice = context.physical_device();
    for(const VkFormat format : candidateDepthFormats) {
        VkFormatProperties properties {};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
        if((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0U) {
            return format;
        }
    }
    throw std::runtime_error {"Failed to find supported depth format"};
}

auto DepthResources::create(const VulkanContext& context, VkFormat format, VkExtent2D extent, std::uint32_t imageCount) -> DepthResources {
    return DepthResources {context, format, extent, imageCount};
}

void DepthResources::recreate(const VulkanContext& context, VkFormat format, VkExtent2D extent, std::uint32_t imageCount) {
    cleanup();
    initialise(context, format, extent, imageCount);
}

auto DepthResources::format() const noexcept -> VkFormat {
    return m_format;
}

auto DepthResources::image_views() const noexcept -> const std::vector<VkImageView>& {
    return m_imageViews;
}

void DepthResources::initialise(const VulkanContext& context, VkFormat format, VkExtent2D extent, std::uint32_t imageCount) {
    if(imageCount == 0U) {
        throw std::invalid_argument {"Depth image count must be greater than zero"};
    }

    m_device = context.device();
    m_format = format;

    m_images.resize(imageCount, VK_NULL_HANDLE);
    m_memories.resize(imageCount, VK_NULL_HANDLE);
    m_imageViews.resize(imageCount, VK_NULL_HANDLE);

    for(std::uint32_t index {0U}; index < imageCount; ++index) {
        VkImageCreateInfo imageInfo {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = extent.width;
        imageInfo.extent.height = extent.height;
        imageInfo.extent.depth = 1U;
        imageInfo.mipLevels = 1U;
        imageInfo.arrayLayers = 1U;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if(vkCreateImage(m_device, &imageInfo, nullptr, &m_images.at(index)) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create depth image"};
        }
        const std::string imageName = "Depth Image " + std::to_string(index);
        context.set_object_name(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<std::uint64_t>(m_images.at(index)), imageName);

        VkMemoryRequirements requirements {};
        vkGetImageMemoryRequirements(m_device, m_images.at(index), &requirements);

        VkMemoryAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = requirements.size;
        allocInfo.memoryTypeIndex = find_memory_type(
            context.physical_device(),
            requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if(vkAllocateMemory(m_device, &allocInfo, nullptr, &m_memories.at(index)) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to allocate depth image memory"};
        }

        if(vkBindImageMemory(m_device, m_images.at(index), m_memories.at(index), 0U) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to bind depth image memory"};
        }

        VkImageViewCreateInfo viewInfo {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_images.at(index);
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if(has_stencil_component(format)) {
            viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        viewInfo.subresourceRange.baseMipLevel = 0U;
        viewInfo.subresourceRange.levelCount = 1U;
        viewInfo.subresourceRange.baseArrayLayer = 0U;
        viewInfo.subresourceRange.layerCount = 1U;

        if(vkCreateImageView(m_device, &viewInfo, nullptr, &m_imageViews.at(index)) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create depth image view"};
        }
        const std::string viewName = "Depth Image View " + std::to_string(index);
        context.set_object_name(VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<std::uint64_t>(m_imageViews.at(index)), viewName);
    }
}

void DepthResources::cleanup() noexcept {
    if(m_device == VK_NULL_HANDLE) {
        return;
    }
    for(VkImageView view : m_imageViews) {
        if(view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, view, nullptr);
        }
    }
    for(VkImage image : m_images) {
        if(image != VK_NULL_HANDLE) {
            vkDestroyImage(m_device, image, nullptr);
        }
    }
    for(VkDeviceMemory memory : m_memories) {
        if(memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, memory, nullptr);
        }
    }
    m_imageViews.clear();
    m_images.clear();
    m_memories.clear();
    m_device = VK_NULL_HANDLE;
}

void DepthResources::move_from(DepthResources&& other) noexcept {
    m_device = other.m_device;
    m_format = other.m_format;
    m_images = std::move(other.m_images);
    m_memories = std::move(other.m_memories);
    m_imageViews = std::move(other.m_imageViews);

    other.m_device = VK_NULL_HANDLE;
    other.m_images.clear();
    other.m_memories.clear();
    other.m_imageViews.clear();
}

} // namespace vulkano
