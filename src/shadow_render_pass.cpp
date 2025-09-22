#include <vulkano/shadow_render_pass.hpp>

#include <stdexcept>

namespace vulkano {

ShadowRenderPass::ShadowRenderPass(const VulkanContext& context, VkFormat depthFormat) {
    initialise(context, depthFormat);
}

ShadowRenderPass::ShadowRenderPass(ShadowRenderPass&& other) noexcept {
    move_from(std::move(other));
}

auto ShadowRenderPass::operator=(ShadowRenderPass&& other) noexcept -> ShadowRenderPass& {
    if(this != &other) {
        cleanup();
        move_from(std::move(other));
    }
    return *this;
}

ShadowRenderPass::~ShadowRenderPass() noexcept {
    cleanup();
}

auto ShadowRenderPass::create(const VulkanContext& context, VkFormat depthFormat) -> ShadowRenderPass {
    return ShadowRenderPass {context, depthFormat};
}

void ShadowRenderPass::recreate(const VulkanContext& context, VkFormat depthFormat) {
    cleanup();
    initialise(context, depthFormat);
}

void ShadowRenderPass::cleanup() noexcept {
    if(m_device != VK_NULL_HANDLE && m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    }
    m_renderPass = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
}

auto ShadowRenderPass::handle() const noexcept -> VkRenderPass {
    return m_renderPass;
}

void ShadowRenderPass::initialise(const VulkanContext& context, VkFormat depthFormat) {
    m_device = context.device();

    VkAttachmentDescription depthAttachment {};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference {};
    depthReference.attachment = 0U;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0U;
    subpass.pDepthStencilAttachment = &depthReference;

    VkSubpassDependency dependency {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0U;
    dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 1U;
    createInfo.pAttachments = &depthAttachment;
    createInfo.subpassCount = 1U;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1U;
    createInfo.pDependencies = &dependency;

    if(vkCreateRenderPass(m_device, &createInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create shadow render pass"};
    }

    context.set_object_name(
        VK_OBJECT_TYPE_RENDER_PASS,
        reinterpret_cast<std::uint64_t>(m_renderPass),
        "Shadow Render Pass");
}

void ShadowRenderPass::move_from(ShadowRenderPass&& other) noexcept {
    m_device = other.m_device;
    m_renderPass = other.m_renderPass;

    other.m_device = VK_NULL_HANDLE;
    other.m_renderPass = VK_NULL_HANDLE;
}

} // namespace vulkano

