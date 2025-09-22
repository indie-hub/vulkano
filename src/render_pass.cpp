#include <vulkano/render_pass.hpp>

#include <array>
#include <stdexcept>

namespace vulkano {

RenderPass::RenderPass(const VulkanContext& context, VkFormat swapchainFormat, VkFormat depthFormat) {
    initialise(context, swapchainFormat, depthFormat);
}

RenderPass::RenderPass(RenderPass&& other) noexcept {
    move_from(std::move(other));
}

auto RenderPass::operator=(RenderPass&& other) noexcept -> RenderPass& {
    if(this != &other) {
        cleanup();
        move_from(std::move(other));
    }
    return *this;
}

RenderPass::~RenderPass() noexcept {
    cleanup();
}

auto RenderPass::create(const VulkanContext& context, VkFormat swapchainFormat, VkFormat depthFormat) -> RenderPass {
    return RenderPass {context, swapchainFormat, depthFormat};
}

auto RenderPass::handle() const noexcept -> VkRenderPass {
    return m_renderPass;
}

void RenderPass::initialise(const VulkanContext& context, VkFormat swapchainFormat, VkFormat depthFormat) {
    m_device = context.device();

    VkAttachmentDescription colorAttachment {};
    colorAttachment.format = swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment {};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef {};
    colorAttachmentRef.attachment = 0U;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef {};
    depthAttachmentRef.attachment = 1U;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1U;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0U;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0U;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2U> attachments {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1U;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1U;
    renderPassInfo.pDependencies = &dependency;

    const VkResult result = vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass);
    if(result != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create render pass"};
    }
    context.set_object_name(VK_OBJECT_TYPE_RENDER_PASS, reinterpret_cast<std::uint64_t>(m_renderPass), "Main Render Pass");
}

void RenderPass::cleanup() noexcept {
    if(m_device != VK_NULL_HANDLE && m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    }
    m_renderPass = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
}

void RenderPass::move_from(RenderPass&& other) noexcept {
    m_device = other.m_device;
    m_renderPass = other.m_renderPass;

    other.m_device = VK_NULL_HANDLE;
    other.m_renderPass = VK_NULL_HANDLE;
}

} // namespace vulkano
