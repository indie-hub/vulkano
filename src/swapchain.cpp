#include <vulkano/swapchain.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include <GLFW/glfw3.h>

namespace {
    struct SwapchainSupportDetails final {
        VkSurfaceCapabilitiesKHR capabilities {};
        std::vector<VkSurfaceFormatKHR> formats {};
        std::vector<VkPresentModeKHR> presentModes {};
    };

    constexpr std::uint32_t imageCountIncrement {1U};

    auto query_support(const vulkano::VulkanContext& context) -> SwapchainSupportDetails {
        SwapchainSupportDetails details {};
        const VkPhysicalDevice physicalDevice = context.physical_device();
        const VkSurfaceKHR surface = context.surface();

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

        std::uint32_t formatCount {0U};
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        if(formatCount != 0U) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
        }

        std::uint32_t presentModeCount {0U};
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
        if(presentModeCount != 0U) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    auto choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) -> VkSurfaceFormatKHR {
        const VkSurfaceFormatKHR preferred {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        for(const VkSurfaceFormatKHR& available : formats) {
            if(available.format == preferred.format && available.colorSpace == preferred.colorSpace) {
                return available;
            }
        }
        if(!formats.empty()) {
            return formats.front();
        }
        return preferred;
    }

    auto choose_present_mode(const std::vector<VkPresentModeKHR>& modes) -> VkPresentModeKHR {
        for(const VkPresentModeKHR mode : modes) {
            if(mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return mode;
            }
        }
        for(const VkPresentModeKHR mode : modes) {
            if(mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR) {
                return mode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    auto clamp_extent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) -> VkExtent2D {
        if(capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
            return capabilities.currentExtent;
        }

        int width {0};
        int height {0};
        glfwGetFramebufferSize(window, &width, &height);
        const VkExtent2D extent {
            static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height)
        };

        VkExtent2D clamped {};
        clamped.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        clamped.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return clamped;
    }

    void create_image_views(
        VkDevice device,
        VkFormat format,
        const std::vector<VkImage>& images,
        std::vector<VkImageView>& imageViews) {
        imageViews.resize(images.size());
        for(std::size_t index {0U}; index < images.size(); ++index) {
            VkImageViewCreateInfo viewInfo {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = images.at(index);
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = format;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0U;
            viewInfo.subresourceRange.levelCount = 1U;
            viewInfo.subresourceRange.baseArrayLayer = 0U;
            viewInfo.subresourceRange.layerCount = 1U;

            const VkResult result = vkCreateImageView(device, &viewInfo, nullptr, &imageViews.at(index));
            if(result != VK_SUCCESS) {
                throw std::runtime_error {"Failed to create swapchain image view"};
            }
        }
    }
} // namespace

namespace vulkano {

SwapchainBuilder::SwapchainBuilder()
    : m_context {nullptr}
    , m_window {nullptr} {
}

auto SwapchainBuilder::with_context(const VulkanContext& context) -> SwapchainBuilder& {
    m_context = &context;
    return *this;
}

auto SwapchainBuilder::with_window(const Window& window) -> SwapchainBuilder& {
    m_window = &window;
    return *this;
}

auto SwapchainBuilder::build() const -> Swapchain {
    if(m_context == nullptr) {
        throw std::runtime_error {"VulkanContext is required to build Swapchain"};
    }
    if(m_window == nullptr) {
        throw std::runtime_error {"Window is required to build Swapchain"};
    }
    return Swapchain {*m_context, *m_window};
}

Swapchain::Swapchain(const VulkanContext& context, const Window& window) {
    initialise(context, window);
}

Swapchain::Swapchain(Swapchain&& other) noexcept {
    move_from(std::move(other));
}

auto Swapchain::operator=(Swapchain&& other) noexcept -> Swapchain& {
    if(this != &other) {
        cleanup();
        move_from(std::move(other));
    }
    return *this;
}

Swapchain::~Swapchain() noexcept {
    cleanup();
}

auto Swapchain::create(const VulkanContext& context, const Window& window) -> Swapchain {
    return Swapchain {context, window};
}

auto Swapchain::handle() const noexcept -> VkSwapchainKHR {
    return m_swapchain;
}

auto Swapchain::image_format() const noexcept -> VkFormat {
    return m_imageFormat;
}

auto Swapchain::extent() const noexcept -> VkExtent2D {
    return m_extent;
}

auto Swapchain::image_views() const noexcept -> const std::vector<VkImageView>& {
    return m_imageViews;
}

void Swapchain::recreate(const VulkanContext& context, const Window& window) {
    vkDeviceWaitIdle(context.device());
    cleanup();
    initialise(context, window);
}

void Swapchain::initialise(const VulkanContext& context, const Window& window) {
    const SwapchainSupportDetails support = query_support(context);
    if(support.formats.empty() || support.presentModes.empty()) {
        throw std::runtime_error {"Swapchain support is insufficient"};
    }

    const VkSurfaceFormatKHR surfaceFormat = choose_surface_format(support.formats);
    const VkPresentModeKHR presentMode = choose_present_mode(support.presentModes);
    const VkExtent2D extent = clamp_extent(support.capabilities, window.handle());

    std::uint32_t imageCount = support.capabilities.minImageCount + imageCountIncrement;
    if(support.capabilities.maxImageCount > 0U && imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = context.surface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1U;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const std::uint32_t graphicsQueueIndex = context.graphics_queue_index();
    const std::uint32_t presentQueueIndex = context.present_queue_index();
    const std::array<std::uint32_t, 2U> queueIndices {graphicsQueueIndex, presentQueueIndex};
    if(graphicsQueueIndex != presentQueueIndex) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = static_cast<std::uint32_t>(queueIndices.size());
        createInfo.pQueueFamilyIndices = queueIndices.data();
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

    VkDevice device = context.device();
    const VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_swapchain);
    if(result != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create swapchain"};
    }

    m_device = device;
    m_imageFormat = surfaceFormat.format;
    m_extent = extent;

    std::uint32_t retrievedImageCount {0U};
    vkGetSwapchainImagesKHR(device, m_swapchain, &retrievedImageCount, nullptr);
    m_images.resize(retrievedImageCount);
    vkGetSwapchainImagesKHR(device, m_swapchain, &retrievedImageCount, m_images.data());

    create_image_views(device, m_imageFormat, m_images, m_imageViews);
}

void Swapchain::cleanup() noexcept {
    if(m_device == VK_NULL_HANDLE) {
        return;
    }

    for(VkImageView view : m_imageViews) {
        if(view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, view, nullptr);
        }
    }
    m_imageViews.clear();
    m_images.clear();

    if(m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
    m_device = VK_NULL_HANDLE;
    m_imageFormat = VK_FORMAT_UNDEFINED;
    m_extent = {0U, 0U};
}

void Swapchain::move_from(Swapchain&& other) noexcept {
    m_swapchain = other.m_swapchain;
    m_device = other.m_device;
    m_imageFormat = other.m_imageFormat;
    m_extent = other.m_extent;
    m_images = std::move(other.m_images);
    m_imageViews = std::move(other.m_imageViews);

    other.m_swapchain = VK_NULL_HANDLE;
    other.m_device = VK_NULL_HANDLE;
    other.m_imageFormat = VK_FORMAT_UNDEFINED;
    other.m_extent = {0U, 0U};
    other.m_images.clear();
    other.m_imageViews.clear();
}

} // namespace vulkano
