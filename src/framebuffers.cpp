#include <vulkano/framebuffers.hpp>

#include <array>
#include <stdexcept>
#include <string>

namespace vulkano {

FramebufferCollection::FramebufferCollection(
    const VulkanContext& context,
    const Swapchain& swapchain,
    const RenderPass& renderPass,
    const std::vector<VkImageView>& depthViews) {
    initialise(context, swapchain, renderPass, depthViews);
}

FramebufferCollection::FramebufferCollection(FramebufferCollection&& other) noexcept {
    move_from(std::move(other));
}

auto FramebufferCollection::operator=(FramebufferCollection&& other) noexcept -> FramebufferCollection& {
    if(this != &other) {
        cleanup();
        move_from(std::move(other));
    }
    return *this;
}

FramebufferCollection::~FramebufferCollection() noexcept {
    cleanup();
}

auto FramebufferCollection::create(
    const VulkanContext& context,
    const Swapchain& swapchain,
    const RenderPass& renderPass,
    const std::vector<VkImageView>& depthViews) -> FramebufferCollection {
    return FramebufferCollection {context, swapchain, renderPass, depthViews};
}

auto FramebufferCollection::handles() const noexcept -> const std::vector<VkFramebuffer>& {
    return m_framebuffers;
}

auto FramebufferCollection::size() const noexcept -> std::size_t {
    return m_framebuffers.size();
}

void FramebufferCollection::recreate(
    const VulkanContext& context,
    const Swapchain& swapchain,
    const RenderPass& renderPass,
    const std::vector<VkImageView>& depthViews) {
    cleanup();
    initialise(context, swapchain, renderPass, depthViews);
}

void FramebufferCollection::initialise(
    const VulkanContext& context,
    const Swapchain& swapchain,
    const RenderPass& renderPass,
    const std::vector<VkImageView>& depthViews) {
    m_device = context.device();
    const VkExtent2D extent = swapchain.extent();

    const auto& imageViews = swapchain.image_views();
    m_framebuffers.resize(imageViews.size());
    if(imageViews.size() != depthViews.size()) {
        throw std::invalid_argument {"Depth views count must match swapchain image count"};
    }
    for(std::size_t index {0U}; index < imageViews.size(); ++index) {
        const std::array<VkImageView, 2U> attachments {imageViews.at(index), depthViews.at(index)};

        VkFramebufferCreateInfo framebufferInfo {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass.handle();
        framebufferInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1U;

        const VkResult result = vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffers.at(index));
        if(result != VK_SUCCESS) {
            throw std::runtime_error {"Failed to create framebuffer"};
        }
        const std::string name = "Framebuffer " + std::to_string(index);
        context.set_object_name(VK_OBJECT_TYPE_FRAMEBUFFER, reinterpret_cast<std::uint64_t>(m_framebuffers.at(index)), name);
    }
}

void FramebufferCollection::cleanup() noexcept {
    if(m_device == VK_NULL_HANDLE) {
        return;
    }
    for(VkFramebuffer framebuffer : m_framebuffers) {
        if(framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }
    }
    m_framebuffers.clear();
    m_device = VK_NULL_HANDLE;
}

void FramebufferCollection::move_from(FramebufferCollection&& other) noexcept {
    m_device = other.m_device;
    m_framebuffers = std::move(other.m_framebuffers);
    other.m_device = VK_NULL_HANDLE;
    other.m_framebuffers.clear();
}

} // namespace vulkano
