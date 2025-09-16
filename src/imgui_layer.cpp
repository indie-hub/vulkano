#include <vulkano_codex/imgui_layer.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

#include <cassert>
#include <array>

namespace vulkano_codex {

namespace {
    VkDescriptorPool create_descriptor_pool(VkDevice device) {
        std::array<VkDescriptorPoolSize, 11> pool_sizes {
            VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
            VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
            VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
            VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
            VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
            VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
            VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
            VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
            VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
        };
        const VkDescriptorPoolCreateInfo info {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = 1000 * static_cast<uint32_t>(pool_sizes.size()),
            .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
            .pPoolSizes = pool_sizes.data(),
        };
        VkDescriptorPool pool {VK_NULL_HANDLE};
        const VkResult res = vkCreateDescriptorPool(device, &info, nullptr, &pool);
        assert(res == VK_SUCCESS);
        return pool;
    }
}

ImGuiLayer::~ImGuiLayer() {
    // must call shutdown() explicitly from App with device handle
}

void ImGuiLayer::init(VkInstance instance,
                      VkPhysicalDevice physical_device,
                      VkDevice device,
                      uint32_t queue_family_index,
                      VkQueue queue,
                      VkRenderPass render_pass,
                      uint32_t image_count,
                      void* glfw_window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(glfw_window), true);

    descriptor_pool_ = create_descriptor_pool(device);

    ImGui_ImplVulkan_InitInfo init_info {};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physical_device;
    init_info.Device = device;
    init_info.QueueFamily = queue_family_index;
    init_info.Queue = queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptor_pool_;
    init_info.Allocator = nullptr;
    init_info.MinImageCount = image_count;
    init_info.ImageCount = image_count;
    init_info.CheckVkResultFn = nullptr;
    init_info.RenderPass = render_pass;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Subpass = 0;
    init_info.UseDynamicRendering = false;
    ImGui_ImplVulkan_Init(&init_info);

    initialized_ = true;
}

void ImGuiLayer::begin_frame() noexcept {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::end_frame(VkCommandBuffer cmd) noexcept {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

// Fonts texture is created automatically on first NewFrame in recent ImGui versions.

void ImGuiLayer::shutdown(VkDevice device) {
    if (!initialized_) {
        return;
    }
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
    }
    initialized_ = false;
}

} // namespace vulkano_codex
