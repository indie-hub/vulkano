#include <cstdint>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>

#include <vulkan_app/imgui_layer.hpp>
#include <vulkan_app/types.hpp>

namespace vulkan_app {

struct ImGuiLayer::Impl final {
    GLFWwindow* window{};
    VkInstance instance{};
    VkDevice device{};
    VkPhysicalDevice gpu{};
    VkQueue queue{};
    std::uint32_t queueFamily{0U};
    VkRenderPass renderPass{};
    VkDescriptorPool imguiPool{};
};

ImGuiLayer::ImGuiLayer(GLFWwindow* window,
                       VkInstance instance,
                       VkDevice device,
                       VkPhysicalDevice gpu,
                       VkQueue graphicsQueue,
                       std::uint32_t graphicsQueueFamily,
                       VkRenderPass renderPass) {
    impl_ = new Impl{};
    impl_->window = window;
    impl_->instance = instance;
    impl_->device = device;
    impl_->gpu = gpu;
    impl_->queue = graphicsQueue;
    impl_->queueFamily = graphicsQueueFamily;
    impl_->renderPass = renderPass;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window, true);

    // Create a small descriptor pool for ImGui
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * static_cast<std::uint32_t>(std::size(pool_sizes));
    pool_info.poolSizeCount = static_cast<std::uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;
    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &impl_->imguiPool) != VK_SUCCESS) {
        IM_ASSERT(false && "Failed to create ImGui descriptor pool");
    }

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = instance;
    init_info.PhysicalDevice = gpu;
    init_info.Device = device;
    init_info.QueueFamily = graphicsQueueFamily;
    init_info.Queue = graphicsQueue;
    init_info.DescriptorPool = impl_->imguiPool;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.UseDynamicRendering = false;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.Allocator = nullptr;
    ImGui_ImplVulkan_Init(&init_info, renderPass);

    // Upload fonts using a one-time command buffer
    VkCommandPool temp_pool{};
    VkCommandPoolCreateInfo cp{}; cp.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cp.queueFamilyIndex = graphicsQueueFamily; cp.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    vkCreateCommandPool(device, &cp, nullptr, &temp_pool);
    VkCommandBuffer cmd{};
    VkCommandBufferAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; ai.commandPool = temp_pool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
    vkAllocateCommandBuffers(device, &ai, &cmd);
    VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    ImGui_ImplVulkan_CreateFontsTexture(cmd);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    ImGui_ImplVulkan_DestroyFontUploadObjects();
    vkDestroyCommandPool(device, temp_pool, nullptr);
}

ImGuiLayer::~ImGuiLayer() noexcept {
    ImGui_ImplVulkan_Shutdown();
    if (impl_->imguiPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(impl_->device, impl_->imguiPool, nullptr);
        impl_->imguiPool = VK_NULL_HANDLE;
    }
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    delete impl_;
}

void ImGuiLayer::new_frame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::draw_ui(int& subdivisions, Light& light, Material& material, SsaoParams& ssao) {
    ImGui::Begin("Vulkan App");
    ImGui::Text("Stats");
    ImGui::Separator();
    ImGui::SliderInt("Subdivisions", &subdivisions, 0, 6);

    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position", &light.position.x, 0.05F);
        ImGui::ColorEdit3("Color", &light.color.x);
        ImGui::DragFloat("Intensity", &light.intensity, 0.05F, 0.0F, 100.0F);
        ImGui::DragFloat("Shininess", &light.shininess, 1.0F, 1.0F, 512.0F);
    }

    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Albedo", &material.albedo.x);
        ImGui::DragFloat("Normal Strength", &material.normalStrength, 0.01F, 0.0F, 2.0F);
    }

    if (ImGui::CollapsingHeader("SSAO", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enabled", reinterpret_cast<bool*>(&ssao.enabled));
        ImGui::SliderInt("Kernel Size", &ssao.kernelSize, 16, 64);
        ImGui::DragFloat("Radius", &ssao.radius, 0.01F, 0.01F, 2.0F);
        ImGui::DragFloat("Bias", &ssao.bias, 0.001F, 0.001F, 0.1F, "%.3f");
        ImGui::DragFloat("Power", &ssao.power, 0.01F, 0.0F, 3.0F);
        ImGui::Checkbox("Blur", reinterpret_cast<bool*>(&ssao.enableBlur));
        ImGui::SliderInt("Blur Radius", &ssao.blurRadius, 1, 5);
    }

    ImGui::End();
}

void ImGuiLayer::render(VkCommandBuffer cmd) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

} // namespace vulkan_app
