#include <vulkano/app/imgui_renderer.hpp>

#include <vulkano/app/vulkan_context.hpp>
#include <vulkano/app/window.hpp>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_backend/imgui_impl_vulkan.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <stdexcept>

namespace {
constexpr std::uint32_t maxDescriptorSets {1024U};
}

namespace vulkano::app {
ImGuiRenderer::ImGuiRenderer(const VulkanContext& context, const Window& window, VkRenderPass renderPass,
    std::uint32_t colorAttachmentCount)
    : m_context {context}
    , m_renderPass {renderPass} {
    create_descriptor_pool();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    configure_style();

    ImGui_ImplGlfw_InitForVulkan(window.handle(), true);

    ImGui_ImplVulkan_InitInfo initInfo {};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = m_context.instance();
    initInfo.PhysicalDevice = m_context.physical_device();
    initInfo.Device = m_context.device();
    initInfo.QueueFamily = m_context.graphics_queue_family_index();
    initInfo.Queue = m_context.graphics_queue();
    initInfo.DescriptorPool = m_descriptorPool;
    initInfo.RenderPass = m_renderPass;
    initInfo.MinImageCount = static_cast<std::uint32_t>(m_context.swapchain_image_views().size());
    initInfo.ImageCount = static_cast<std::uint32_t>(m_context.swapchain_image_views().size());
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.Subpass = 0U;
    initInfo.ColorAttachmentCount = colorAttachmentCount;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = nullptr;
    initInfo.UseDynamicRendering = false;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        throw std::runtime_error {"Failed to initialise ImGui Vulkan backend"};
    }
}

ImGuiRenderer::~ImGuiRenderer() noexcept {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    destroy_descriptor_pool();
}

void ImGuiRenderer::begin_frame() noexcept {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiRenderer::end_frame() noexcept {
    ImGui::Render();
}

void ImGuiRenderer::render(VkCommandBuffer commandBuffer) const {
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void ImGuiRenderer::update_metrics(float deltaTimeSeconds) noexcept {
    if (deltaTimeSeconds <= 0.0F) {
        return;
    }
    m_frameTimeMilliseconds = deltaTimeSeconds * 1000.0F;
    m_framesPerSecond = 1.0F / deltaTimeSeconds;
}

void ImGuiRenderer::draw_overlay() const noexcept {
    ImGui::SetNextWindowPos(ImVec2 {10.0F, 10.0F}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.35F);
    if (ImGui::Begin("Diagnostics", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("Frame: %.2f ms", m_frameTimeMilliseconds);
        ImGui::Text("FPS: %.1f", m_framesPerSecond);
    }
    ImGui::End();
}

void ImGuiRenderer::create_descriptor_pool() {
    std::array<VkDescriptorPoolSize, 11> poolSizes {VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_SAMPLER, maxDescriptorSets},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxDescriptorSets},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, maxDescriptorSets},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, maxDescriptorSets},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, maxDescriptorSets},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, maxDescriptorSets},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxDescriptorSets},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, maxDescriptorSets},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, maxDescriptorSets},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, maxDescriptorSets},
        VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, maxDescriptorSets}};

    VkDescriptorPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = maxDescriptorSets * static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(m_context.device(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error {"Failed to create ImGui descriptor pool"};
    }
}

void ImGuiRenderer::destroy_descriptor_pool() noexcept {
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_context.device(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
}

void ImGuiRenderer::configure_style() noexcept {
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}
} // namespace vulkano::app
