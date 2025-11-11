#include <vulkano/app/shadow_map.hpp>

#include <vulkano/app/vulkan_context.hpp>

#include <array>
#include <stdexcept>

namespace vulkano::app {
namespace {
[[nodiscard]] VkSampler create_shadow_sampler(VkDevice device) {
    VkSamplerCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    info.compareEnable = VK_FALSE;
    info.compareOp = VK_COMPARE_OP_ALWAYS;
    info.minLod = 0.0F;
    info.maxLod = 0.0F;

    VkSampler sampler {VK_NULL_HANDLE};
    if (vkCreateSampler(device, &info, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create shadow map sampler"};
    }
    return sampler;
}

[[nodiscard]] VkRenderPass create_render_pass(VkDevice device, VkFormat depthFormat) {
    VkAttachmentDescription depthAttachment {};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef {};
    depthRef.attachment = 0U;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depthRef;

    std::array<VkSubpassDependency, 2> dependencies {};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0U;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    dependencies[1].srcSubpass = 0U;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1U;
    info.pAttachments = &depthAttachment;
    info.subpassCount = 1U;
    info.pSubpasses = &subpass;
    info.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
    info.pDependencies = dependencies.data();

    VkRenderPass renderPass {VK_NULL_HANDLE};
    if (vkCreateRenderPass(device, &info, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create shadow render pass"};
    }
    return renderPass;
}

[[nodiscard]] VkFramebuffer create_framebuffer(VkDevice device, VkRenderPass renderPass, VkImageView depthView,
    VkExtent2D extent) {
    VkFramebufferCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = renderPass;
    info.attachmentCount = 1U;
    info.pAttachments = &depthView;
    info.width = extent.width;
    info.height = extent.height;
    info.layers = 1U;

    VkFramebuffer framebuffer {VK_NULL_HANDLE};
    if (vkCreateFramebuffer(device, &info, nullptr, &framebuffer) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create shadow framebuffer"};
    }
    return framebuffer;
}
} // namespace

ShadowMap::ShadowMap(const VulkanContext& context, VkExtent2D extent, VkFormat depthFormat) {
    create_shadow_resources(context, extent, depthFormat);
}

ShadowMap::~ShadowMap() noexcept {
    destroy();
}

ShadowMap::ShadowMap(ShadowMap&& other) noexcept
    : m_context {other.m_context}
    , m_renderPass {other.m_renderPass}
    , m_framebuffer {other.m_framebuffer}
    , m_depthImage {std::move(other.m_depthImage)}
    , m_sampler {other.m_sampler}
    , m_extent {other.m_extent} {
    other.m_context = nullptr;
    other.m_renderPass = VK_NULL_HANDLE;
    other.m_framebuffer = VK_NULL_HANDLE;
    other.m_sampler = VK_NULL_HANDLE;
    other.m_extent = VkExtent2D {0U, 0U};
}

ShadowMap& ShadowMap::operator=(ShadowMap&& other) noexcept {
    if (this != &other) {
        destroy();
        m_renderPass = other.m_renderPass;
        m_framebuffer = other.m_framebuffer;
        m_depthImage = std::move(other.m_depthImage);
        m_sampler = other.m_sampler;
        m_extent = other.m_extent;
        m_context = other.m_context;

        other.m_context = nullptr;
        other.m_renderPass = VK_NULL_HANDLE;
        other.m_framebuffer = VK_NULL_HANDLE;
        other.m_sampler = VK_NULL_HANDLE;
        other.m_extent = VkExtent2D {0U, 0U};
    }
    return *this;
}

void ShadowMap::recreate(const VulkanContext& context, VkExtent2D extent, VkFormat depthFormat) {
    destroy();
    create_shadow_resources(context, extent, depthFormat);
}

VkFramebuffer ShadowMap::framebuffer() const noexcept {
    return m_framebuffer;
}

VkRenderPass ShadowMap::render_pass() const noexcept {
    return m_renderPass;
}

VkExtent2D ShadowMap::extent() const noexcept {
    return m_extent;
}

VkDescriptorImageInfo ShadowMap::descriptor_info() const noexcept {
    VkDescriptorImageInfo info {};
    info.sampler = m_sampler;
    info.imageView = m_depthImage.view();
    info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    return info;
}

VkImage ShadowMap::image() const noexcept {
    return m_depthImage.image();
}

VkImageView ShadowMap::view() const noexcept {
    return m_depthImage.view();
}

void ShadowMap::destroy() noexcept {
    if (m_context == nullptr) {
        return;
    }
    VkDevice device = m_context->device();
    if (m_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, m_framebuffer, nullptr);
        m_framebuffer = VK_NULL_HANDLE;
    }
    m_depthImage = vk::DepthImage {};
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
    m_depthImage = vk::DepthImage {};
    m_context = nullptr;
}

void ShadowMap::create_shadow_resources(const VulkanContext& context, VkExtent2D extent, VkFormat depthFormat) {
    m_context = &context;
    m_extent = extent;
    const VkDevice device = context.device();

    const VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    m_depthImage = vk::DepthImage::create(context.physical_device(), device, extent, depthFormat, usage);

    m_renderPass = create_render_pass(device, depthFormat);
    m_framebuffer = create_framebuffer(device, m_renderPass, m_depthImage.view(), extent);
    m_sampler = create_shadow_sampler(device);
}
} // namespace vulkano::app
