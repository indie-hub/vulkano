#include <vulkano/vk/swapchain.hpp>

#include <vulkano/app/window.hpp>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <limits>

namespace {
VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const VkSurfaceFormatKHR& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return availableFormats.front();
}

VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const VkPresentModeKHR presentMode : availablePresentModes) {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return presentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities, const vulkano::app::Window& window) {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D actualExtent = window.framebuffer_extent();
    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return actualExtent;
}

std::vector<VkImageView> create_image_views(VkDevice device, VkFormat format, const std::vector<VkImage>& images) {
    std::vector<VkImageView> imageViews;
    imageViews.reserve(images.size());

    for (VkImage image : images) {
        VkImageViewCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = format;
        createInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY};
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0U;
        createInfo.subresourceRange.levelCount = 1U;
        createInfo.subresourceRange.baseArrayLayer = 0U;
        createInfo.subresourceRange.layerCount = 1U;

        VkImageView imageView {VK_NULL_HANDLE};
        if (vkCreateImageView(device, &createInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create image view"};
        }
        imageViews.push_back(imageView);
    }

    return imageViews;
}
}

namespace vulkano::vk {
SwapchainManager SwapchainManager::create(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
    const QueueFamilySelection& queues, const app::Window& window) {
    const SwapchainSupport support = query_swapchain_support(physicalDevice, surface);
    if (support.formats.empty() || support.presentModes.empty()) {
        throw std::runtime_error {"Swap chain support incomplete"};
    }

    const VkSurfaceFormatKHR surfaceFormat = choose_surface_format(support.formats);
    const VkPresentModeKHR presentMode = choose_present_mode(support.presentModes);
    const VkExtent2D extent = choose_extent(support.capabilities, window);

    std::uint32_t imageCount = support.capabilities.minImageCount + 1U;
    if (support.capabilities.maxImageCount > 0U && imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1U;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (queues.graphicsFamily != queues.presentFamily) {
        const std::array<std::uint32_t, 2> queueFamilyIndices {queues.graphicsFamily, queues.presentFamily};
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = static_cast<std::uint32_t>(queueFamilyIndices.size());
        createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0U;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain {VK_NULL_HANDLE};
    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create swap chain"};
    }

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());

    std::vector<VkImageView> imageViews = create_image_views(device, surfaceFormat.format, images);

    return SwapchainManager {device, swapchain, surfaceFormat.format, extent, std::move(images), std::move(imageViews)};
}

SwapchainManager::SwapchainManager(VkDevice device, VkSwapchainKHR swapchain, VkFormat format, VkExtent2D extent,
    std::vector<VkImage> images, std::vector<VkImageView> imageViews) noexcept
    : m_device {device}
    , m_swapchain {swapchain}
    , m_format {format}
    , m_extent {extent}
    , m_images {std::move(images)}
    , m_imageViews {std::move(imageViews)} {
}

SwapchainManager::SwapchainManager(SwapchainManager&& other) noexcept
    : m_device {other.m_device}
    , m_swapchain {other.m_swapchain}
    , m_format {other.m_format}
    , m_extent {other.m_extent}
    , m_images {std::move(other.m_images)}
    , m_imageViews {std::move(other.m_imageViews)} {
    other.m_device = VK_NULL_HANDLE;
    other.m_swapchain = VK_NULL_HANDLE;
    other.m_format = VK_FORMAT_UNDEFINED;
    other.m_extent = VkExtent2D {0U, 0U};
}

SwapchainManager& SwapchainManager::operator=(SwapchainManager&& other) noexcept {
    if (this != &other) {
        if (m_device != VK_NULL_HANDLE && m_swapchain != VK_NULL_HANDLE) {
            for (VkImageView view : m_imageViews) {
                if (view != VK_NULL_HANDLE) {
                    vkDestroyImageView(m_device, view, nullptr);
                }
            }
            m_imageViews.clear();
            vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        }

        m_device = other.m_device;
        m_swapchain = other.m_swapchain;
        m_format = other.m_format;
        m_extent = other.m_extent;
        m_images = std::move(other.m_images);
        m_imageViews = std::move(other.m_imageViews);

        other.m_device = VK_NULL_HANDLE;
        other.m_swapchain = VK_NULL_HANDLE;
        other.m_format = VK_FORMAT_UNDEFINED;
        other.m_extent = VkExtent2D {0U, 0U};
    }
    return *this;
}

SwapchainManager::~SwapchainManager() noexcept {
    if (m_device != VK_NULL_HANDLE && m_swapchain != VK_NULL_HANDLE) {
        for (VkImageView view : m_imageViews) {
            if (view != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device, view, nullptr);
            }
        }
        m_imageViews.clear();
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

VkSwapchainKHR SwapchainManager::handle() const noexcept {
    return m_swapchain;
}

VkFormat SwapchainManager::image_format() const noexcept {
    return m_format;
}

VkExtent2D SwapchainManager::extent() const noexcept {
    return m_extent;
}

const std::vector<VkImage>& SwapchainManager::images() const noexcept {
    return m_images;
}

const std::vector<VkImageView>& SwapchainManager::image_views() const noexcept {
    return m_imageViews;
}
} // namespace vulkano::vk
