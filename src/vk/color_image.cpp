#include <vulkano/vk/color_image.hpp>

#include <stdexcept>

namespace {
VkImage create_image(VkDevice device, VkExtent2D extent, VkFormat format, VkImageUsageFlags usageFlags) {
    VkImageCreateInfo imageInfo {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent.width = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth = 1U;
    imageInfo.mipLevels = 1U;
    imageInfo.arrayLayers = 1U;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = usageFlags;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image {VK_NULL_HANDLE};
    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create color attachment image"};
    }
    return image;
}
}

namespace vulkano::vk {
ColorImage ColorImage::create(
    VkPhysicalDevice physicalDevice, VkDevice device, VkExtent2D extent, VkFormat format, VkImageUsageFlags usageFlags) {
    const VkImage image = ::create_image(device, extent, format, usageFlags);

    VkMemoryRequirements requirements {};
    vkGetImageMemoryRequirements(device, image, &requirements);

    VkMemoryAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = find_memory_type(physicalDevice, requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory memory {VK_NULL_HANDLE};
    if (vkAllocateMemory(device, &allocateInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyImage(device, image, nullptr);
        throw std::runtime_error {"Failed to allocate color attachment memory"};
    }

    if (vkBindImageMemory(device, image, memory, 0U) != VK_SUCCESS) {
        vkFreeMemory(device, memory, nullptr);
        vkDestroyImage(device, image, nullptr);
        throw std::runtime_error {"Failed to bind color attachment memory"};
    }

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

    VkImageView view {VK_NULL_HANDLE};
    if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
        vkFreeMemory(device, memory, nullptr);
        vkDestroyImage(device, image, nullptr);
        throw std::runtime_error {"Failed to create color attachment image view"};
    }

    return ColorImage {device, image, memory, view, format, extent};
}

ColorImage::ColorImage(VkDevice device, VkImage image, VkDeviceMemory memory, VkImageView view, VkFormat format,
    VkExtent2D extent) noexcept
    : m_device {device}
    , m_image {image}
    , m_memory {memory}
    , m_view {view}
    , m_format {format}
    , m_extent {extent} {
}

ColorImage::ColorImage(ColorImage&& other) noexcept
    : m_device {other.m_device}
    , m_image {other.m_image}
    , m_memory {other.m_memory}
    , m_view {other.m_view}
    , m_format {other.m_format}
    , m_extent {other.m_extent} {
    other.m_device = VK_NULL_HANDLE;
    other.m_image = VK_NULL_HANDLE;
    other.m_memory = VK_NULL_HANDLE;
    other.m_view = VK_NULL_HANDLE;
    other.m_format = VK_FORMAT_UNDEFINED;
    other.m_extent = VkExtent2D {0U, 0U};
}

ColorImage& ColorImage::operator=(ColorImage&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy();

    m_device = other.m_device;
    m_image = other.m_image;
    m_memory = other.m_memory;
    m_view = other.m_view;
    m_format = other.m_format;
    m_extent = other.m_extent;

    other.m_device = VK_NULL_HANDLE;
    other.m_image = VK_NULL_HANDLE;
    other.m_memory = VK_NULL_HANDLE;
    other.m_view = VK_NULL_HANDLE;
    other.m_format = VK_FORMAT_UNDEFINED;
    other.m_extent = VkExtent2D {0U, 0U};

    return *this;
}

ColorImage::~ColorImage() noexcept {
    destroy();
}

VkImage ColorImage::image() const noexcept {
    return m_image;
}

VkImageView ColorImage::view() const noexcept {
    return m_view;
}

VkFormat ColorImage::format() const noexcept {
    return m_format;
}

VkExtent2D ColorImage::extent() const noexcept {
    return m_extent;
}

void ColorImage::destroy() noexcept {
    if (m_device == VK_NULL_HANDLE) {
        return;
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
    m_format = VK_FORMAT_UNDEFINED;
    m_extent = VkExtent2D {0U, 0U};
}

std::uint32_t ColorImage::find_memory_type(
    VkPhysicalDevice physicalDevice, std::uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties {};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (std::uint32_t i {0U}; i < memoryProperties.memoryTypeCount; ++i) {
        const bool typeSupported = (typeFilter & (1U << i)) != 0U;
        const bool flagsSupported = (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
        if (typeSupported && flagsSupported) {
            return i;
        }
    }

    throw std::runtime_error {"Failed to find suitable memory type for color attachment"};
}
} // namespace vulkano::vk
